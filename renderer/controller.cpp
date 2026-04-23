#include "controller.h"
#include "card_io.h"
#include "effect.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static const float DEFAULT_PEAK_THRESHOLD = 0.05f;
static const uint32_t SAMPLE_RATES[] = {44100, 48000, 96000};
static const uint32_t BUFFER_SIZES[] = {128, 256, 512, 1024, 2048};
static const int N_RATES = 3;
static const int N_BUFS = 5;
static const char* AUDIO_CFG_FILE = "audio_config.json";

struct AppConfig {
    sp303::AudioConfig audio{};
    float peak_threshold = DEFAULT_PEAK_THRESHOLD;
    std::string card_path = "cards/default";
};

static float sample_level_threshold_to_peak(int level) {
    if (level <= 0) return 0.003f;
    return std::clamp(level * 0.01f, 0.0f, 1.0f);
}

static sp303::AudioQuality to_audio_quality(sp303::SampleQuality q) {
    switch (q) {
        case sp303::SAMPLE_QUALITY_LONG: return sp303::AUDIO_QUALITY_LONG;
        case sp303::SAMPLE_QUALITY_LOFI: return sp303::AUDIO_QUALITY_LOFI;
        case sp303::SAMPLE_QUALITY_STANDARD:
        default: return sp303::AUDIO_QUALITY_STANDARD;
    }
}

static float bpm_adjust_to_knob(int adjust) {
    if (adjust < 0) return 0.0f;
    if (adjust > 0) return 1.0f;
    return 0.5f;
}

static float time_mode_to_knob(int mode, int target_bpm, int source_bpm) {
    if (mode == 0) return 0.0f;
    if (mode == 2) return 1.0f;
    source_bpm = std::clamp(source_bpm, 40, 200);
    target_bpm = std::clamp(target_bpm, 40, 200);
    float ratio = target_bpm / (float)source_bpm;
    return std::clamp((ratio - 0.5f) / 0.8f, 0.02f, 0.98f);
}

static bool knob_mode_owns_input_gain(sp303::Device* dev) {
    return sp303::is_sampling_standby(dev) ||
           sp303::is_sampling_ready(dev) ||
           sp303::is_recording(dev) ||
           sp303::is_resampling_mode(dev);
}

static bool knob_mode_owns_effect_params(sp303::Device* dev) {
    return sp303::get_active_effect_btn(dev) != -1 &&
           !sp303::is_start_end_level_mode(dev) &&
           !sp303::is_time_bpm_mode(dev) &&
           !sp303::is_sampling_standby(dev) &&
           !sp303::is_sampling_ready(dev) &&
           !sp303::is_recording(dev) &&
           !sp303::is_threshold_mode(dev) &&
           !sp303::is_delete_mode(dev) &&
           !sp303::is_resampling_mode(dev);
}

static bool virtual_input_exists() {
    FILE* pipe = popen("pactl list sources 2>/dev/null | grep 'SP303_Input'", "r");
    if (!pipe) return false;
    char buffer[256];
    bool found = fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    return found;
}

static AppConfig load_app_config() {
    AppConfig cfg;
    sp303::audio_config_default(&cfg.audio);

    if (virtual_input_exists()) {
        std::strncpy(cfg.audio.input_name, "Monitor of SP303_Input", sp303::AUDIO_NAME_LEN - 1);
    }

    std::ifstream f(AUDIO_CFG_FILE);
    if (!f.is_open()) return cfg;
    try {
        auto j = json::parse(f);
        if (j.contains("output")) std::strncpy(cfg.audio.output_name, j["output"].get<std::string>().c_str(), sp303::AUDIO_NAME_LEN - 1);
        if (j.contains("input") && !virtual_input_exists()) {
            std::strncpy(cfg.audio.input_name, j["input"].get<std::string>().c_str(), sp303::AUDIO_NAME_LEN - 1);
        }
        if (j.contains("rate"))   cfg.audio.sample_rate   = j["rate"];
        if (j.contains("buffer")) cfg.audio.buffer_frames = j["buffer"];
        if (j.contains("peak_threshold")) {
            cfg.peak_threshold = std::clamp(j["peak_threshold"].get<float>(), 0.0f, 1.0f);
        }
        if (j.contains("card_path")) {
            cfg.card_path = j["card_path"].get<std::string>();
        }
    } catch (...) {}
    return cfg;
}

static void save_app_config(const RendererController* c) {
    if (!c) return;
    json j;
    j["output"] = c->audio_cfg.output_name;
    j["input"]  = c->audio_cfg.input_name;
    j["rate"]   = c->audio_cfg.sample_rate;
    j["buffer"] = c->audio_cfg.buffer_frames;
    j["peak_threshold"] = c->peak_threshold;
    j["card_path"] = c->card_path;
    std::ofstream f(AUDIO_CFG_FILE);
    f << j.dump(2);
}

void renderer_controller_refresh_cards(RendererController* c) {
    if (!c) return;
    c->card_dirs.clear();
    std::error_code ec;
    std::filesystem::create_directories("cards", ec);
    for (const auto& entry : std::filesystem::directory_iterator("cards", ec)) {
        if (entry.is_directory()) {
            c->card_dirs.push_back(entry.path().generic_string());
        }
    }
    std::sort(c->card_dirs.begin(), c->card_dirs.end());
    if (c->card_dirs.empty()) {
        c->card_dirs.push_back("cards/default");
        std::filesystem::create_directories(c->card_dirs[0], ec);
    }
    auto it = std::find(c->card_dirs.begin(), c->card_dirs.end(), c->card_path);
    if (it == c->card_dirs.end()) {
        c->card_dirs.push_back(c->card_path);
        std::sort(c->card_dirs.begin(), c->card_dirs.end());
        it = std::find(c->card_dirs.begin(), c->card_dirs.end(), c->card_path);
    }
    c->sel_card = (it != c->card_dirs.end()) ? (int)std::distance(c->card_dirs.begin(), it) : 0;
}

bool renderer_controller_mount_card(RendererController* c, sp303::Device* dev) {
    if (!c || !dev || !c->audio) return false;
    std::error_code ec;
    std::filesystem::create_directories(c->card_path, ec);
    CardIoResult res = card_load(c->card_path, dev, c->audio, c->audio_cfg);
    std::printf("[CARD] %s\n", res.message.c_str());
    return res.ok;
}

static void memory_card_backup_save_samples(sp303::Audio* audio, sp303::Device* dev, int backup_slot) {
    if (!audio || !dev || backup_slot < 0 || backup_slot >= 8) return;
    sp303::set_memory_card_backup_kind(dev, backup_slot, sp303::MEMORY_CARD_BACKUP_SAMPLES);
    for (int slot = 0; slot < 16; ++slot) {
        sp303::PadProjectState state{};
        sp303::get_pad_project_state(dev, slot, &state);
        sp303::set_memory_card_backup_sample_state(dev, backup_slot, slot, state);
        if (!state.has_sample) {
            sp303::set_memory_card_backup_sample_audio(dev, backup_slot, slot, nullptr, 0, 1);
            sp303::set_memory_card_backup_sample_edit(dev, backup_slot, slot, 0, 127, 127, 0, 0, -1);
            continue;
        }
        std::vector<float> pcm;
        uint32_t channels = 1;
        uint32_t frames = 0;
        if (!sp303::audio_export_sample(audio, slot, &pcm, &channels, &frames)) {
            pcm.clear();
            channels = 1;
            frames = 0;
        }
        sp303::set_memory_card_backup_sample_audio(dev, backup_slot, slot, pcm.data(), frames, channels);
        sp303::set_memory_card_backup_sample_edit(
            dev, backup_slot, slot,
            sp303::audio_get_sample_start(audio, slot),
            sp303::audio_get_sample_end(audio, slot),
            sp303::audio_get_sample_level(audio, slot),
            sp303::audio_get_sample_bpm_adjust(audio, slot),
            sp303::audio_get_sample_time_mode(audio, slot),
            sp303::audio_get_sample_time_target_bpm(audio, slot));
    }
}

static void memory_card_backup_load_samples(sp303::Audio* audio, sp303::Device* dev, int backup_slot) {
    if (!audio || !dev || backup_slot < 0 || backup_slot >= 8) return;
    for (int slot = 0; slot < 16; ++slot) {
        sp303::audio_clear_sample(audio, slot);
        sp303::pad_clear_sample(dev, slot);
    }
    for (int slot = 0; slot < 16; ++slot) {
        sp303::PadProjectState state{};
        if (!sp303::get_memory_card_backup_sample_state(dev, backup_slot, slot, &state)) {
            continue;
        }
        if (state.has_sample) {
            std::vector<float> pcm;
            uint32_t channels = 1;
            uint32_t frames = 0;
            if (sp303::get_memory_card_backup_sample_audio(dev, backup_slot, slot, &pcm, &channels, &frames) &&
                !pcm.empty() && frames > 0) {
                sp303::audio_import_sample(audio, slot, pcm.data(), frames, channels);
                int start_127 = 0, end_127 = 127, level_127 = 127;
                int bpm_adjust = 0, time_mode = 0, time_target_bpm = -1;
                sp303::get_memory_card_backup_sample_edit(dev, backup_slot, slot,
                    &start_127, &end_127, &level_127, &bpm_adjust, &time_mode, &time_target_bpm);
                sp303::audio_set_sample_start(audio, slot, start_127);
                sp303::audio_set_sample_end(audio, slot, end_127);
                sp303::audio_set_sample_level(audio, slot, level_127);
                sp303::audio_set_sample_bpm_adjust(audio, slot, bpm_adjust);
                sp303::audio_set_sample_time_mode(audio, slot, time_mode, time_target_bpm);
            } else {
                state.has_sample = false;
            }
        }
        sp303::set_pad_project_state(dev, slot, state);
    }
}

void renderer_controller_refresh_devices(RendererController* c) {
    if (!c) return;
    c->out_devs.clear();
    c->in_devs.clear();
    if (!c->audio) return;

    c->out_devs.resize(64);
    c->in_devs.resize(64);
    int n_out = sp303::audio_list_outputs(c->audio, c->out_devs.data(), 64);
    int n_in  = sp303::audio_list_inputs(c->audio, c->in_devs.data(), 64);
    c->out_devs.resize(n_out);
    c->in_devs.resize(n_in);
}

bool renderer_controller_init(RendererController* c) {
    if (!c) return false;
    AppConfig cfg = load_app_config();
    c->audio_cfg = cfg.audio;
    c->peak_threshold = cfg.peak_threshold;
    c->card_path = cfg.card_path;
    c->audio = sp303::audio_create(&c->audio_cfg);
    c->playback_ok = (c->audio != nullptr);
    renderer_controller_refresh_devices(c);
    renderer_controller_refresh_cards(c);

    for (int i = 0; i < (int)c->out_devs.size(); ++i) {
        if (std::strncmp(c->out_devs[i].name, c->audio_cfg.output_name, sp303::AUDIO_NAME_LEN) == 0) {
            c->sel_out = i;
            break;
        }
    }
    for (int i = 0; i < (int)c->in_devs.size(); ++i) {
        if (std::strncmp(c->in_devs[i].name, c->audio_cfg.input_name, sp303::AUDIO_NAME_LEN) == 0) {
            c->sel_in = i;
            break;
        }
    }
    for (int i = 0; i < N_RATES; ++i) {
        if (SAMPLE_RATES[i] == c->audio_cfg.sample_rate) {
            c->sel_rate = i;
            break;
        }
    }
    for (int i = 0; i < N_BUFS; ++i) {
        if (BUFFER_SIZES[i] == c->audio_cfg.buffer_frames) {
            c->sel_buf = i;
            break;
        }
    }

    std::printf("[SP-303] Output: %s\n", c->out_devs.empty() ? "(none)" : c->out_devs[c->sel_out].name);
    std::printf("[SP-303] Input:  %s\n", c->in_devs.empty() ? "(none)" : c->in_devs[c->sel_in].name);
    c->cached_input_gain = 0.8f;
    c->cached_fx_p1 = 0.5f;
    c->cached_fx_p2 = 0.5f;
    c->cached_fx_p3 = 0.5f;
    return true;
}

void renderer_controller_shutdown(RendererController* c) {
    if (!c) return;
    sp303::audio_destroy(c->audio);
    c->audio = nullptr;
}

bool renderer_controller_apply_audio_config(RendererController* c) {
    if (!c) return false;
    sp303::AudioConfig new_cfg = c->audio_cfg;
    if (!c->out_devs.empty()) {
        std::strncpy(new_cfg.output_name, c->out_devs[c->sel_out].name, sp303::AUDIO_NAME_LEN - 1);
    }
    if (!c->in_devs.empty()) {
        std::strncpy(new_cfg.input_name, c->in_devs[c->sel_in].name, sp303::AUDIO_NAME_LEN - 1);
    }
    new_cfg.sample_rate   = SAMPLE_RATES[c->sel_rate];
    new_cfg.buffer_frames = BUFFER_SIZES[c->sel_buf];

    bool ok = true;
    if (c->audio) {
        ok = sp303::audio_reconfigure(c->audio, &new_cfg);
    }
    c->playback_ok = ok;
    if (ok) {
        c->audio_cfg = new_cfg;
        save_app_config(c);
        renderer_controller_refresh_devices(c);
    }
    return ok;
}

sp303::State renderer_controller_step(RendererController* c, sp303::Device* dev, int active_knob) {
    sp303::tick(dev, 735);
    sp303::State state = sp303::get_state(dev);

    if (!c || !c->audio) return state;

    sp303::Audio* audio = c->audio;
    for (int i = 0; i < 32; ++i) {
        sp303::set_pad_playing(dev, i, sp303::audio_is_playing(audio, i));
    }
    sp303::audio_set_recording_mode(audio,
                                    sp303::get_sampling_stereo(dev),
                                    to_audio_quality(sp303::get_sampling_quality(dev)));
    float input_peak       = sp303::audio_input_peak(audio);
    float output_peak      = sp303::audio_peak(audio);
    float stereo_diff_peak = sp303::audio_stereo_diff_peak(audio);
    float global_peak      = std::max(input_peak, output_peak);
    bool should_light_peak = global_peak > c->peak_threshold;
    c->stereo_activity_lit = (output_peak > 0.01f) && (stereo_diff_peak > 0.01f);

    if (should_light_peak) {
        c->peak_hold_frames = 10;
    }
    if (c->peak_hold_frames > 0) {
        c->peak_hold_frames--;
        sp303::indicator_set(dev, sp303::IND_PEAK, true);
    } else {
        sp303::indicator_set(dev, sp303::IND_PEAK, false);
    }
    c->config_input_peak = input_peak;

    // MFX held: CTRL 3 changes mfx_type instead of effect p3.
    const bool mfx_held = state.buttons[sp303::BTN_MFX].pressed;
    bool input_gain_mode = knob_mode_owns_input_gain(dev);
    bool effect_param_mode = knob_mode_owns_effect_params(dev) && !mfx_held;
    if (effect_param_mode && !c->was_effect_param_mode) {
        // Re-entering effect control: restore the knob-owned FX cache so
        // sample edit / time-bpm / resample gain values do not leak into FX.
        sp303::knob_set(dev, sp303::KNOB_CUTOFF, c->cached_fx_p1);
        sp303::knob_set(dev, sp303::KNOB_RESONANCE, c->cached_fx_p2);
        // Skip KNOB_DRIVE restore while MFX held — it would corrupt mfx_type
        if (!mfx_held)
            sp303::knob_set(dev, sp303::KNOB_DRIVE, c->cached_fx_p3);
        state = sp303::get_state(dev);
    }
    if (input_gain_mode && !c->was_input_gain_mode) {
        sp303::knob_set(dev, sp303::KNOB_DRIVE, c->cached_input_gain);
        if (sp303::is_resample_source_select(dev)) {
            sp303::clear_input_gain_display(dev);
        }
        state = sp303::get_state(dev);
    }
    if (input_gain_mode) {
        c->cached_input_gain = state.knobs[sp303::KNOB_DRIVE].value;
    }
    if (effect_param_mode) {
        c->cached_fx_p1 = state.knobs[sp303::KNOB_CUTOFF].value;
        c->cached_fx_p2 = state.knobs[sp303::KNOB_RESONANCE].value;
        if (!mfx_held)
            c->cached_fx_p3 = state.knobs[sp303::KNOB_DRIVE].value;
    }

    sp303::audio_set_input_gain(audio, c->cached_input_gain);
    sp303::audio_set_output_gain(audio, state.knobs[sp303::KNOB_VOLUME].value);

    // Push effect routing and current knob values to audio engine
    {
        int      active_btn = sp303::get_active_effect_btn(dev);
        int      mfx_sub    = sp303::get_mfx_type(dev);
        uint32_t pad_mask   = 0;
        for (int i = 0; i < 32; ++i) {
            if (sp303::get_pad_has_effect(dev, i))
                pad_mask |= (1u << i);
        }
        sp303::audio_set_effect_routing(audio, active_btn, mfx_sub, pad_mask);
        sp303::audio_set_effect_params(audio,
            c->cached_fx_p1,
            c->cached_fx_p2,
            c->cached_fx_p3);

        // Compute BPM for note-synced delay
        if (active_btn == sp303::BTN_DELAY) {
            float delay_bpm = 120.0f;
            if (sp303::is_pattern_mode(dev)) {
                delay_bpm = static_cast<float>(sp303::get_pattern_bpm(dev));
            } else {
                for (int i = 0; i < 32; ++i) {
                    if (!((pad_mask >> i) & 1u)) continue;
                    int bpm = sp303::audio_get_sample_bpm(audio, i);
                    if (bpm >= 40) { delay_bpm = static_cast<float>(bpm); break; }
                }
            }
            sp303::audio_set_delay_bpm(audio, delay_bpm);
        }
    }
    c->was_input_gain_mode = input_gain_mode;
    c->was_effect_param_mode = effect_param_mode;

    sp303::audio_set_pattern_bpm(audio, sp303::get_pattern_bpm(dev));

    if (sp303::is_pattern_record_select(dev) && !sp303::is_pattern_recording(dev)) {
        if (!c->was_pattern_record_select) {
            c->pattern_preview_beat_index = 0;
            sp303::audio_trigger_metronome(audio, sp303::get_pattern_metronome_level(dev), true);
        }
        const double samples_per_quarter =
            (60.0 * c->audio_cfg.sample_rate) / std::max(40, sp303::get_pattern_bpm(dev));
        c->pattern_preview_quarter_progress += 735.0;
        while (c->pattern_preview_quarter_progress >= samples_per_quarter) {
            c->pattern_preview_quarter_progress -= samples_per_quarter;
            c->pattern_preview_beat_index = (c->pattern_preview_beat_index + 1) % 4;
            sp303::audio_trigger_metronome(audio,
                                           sp303::get_pattern_metronome_level(dev),
                                           c->pattern_preview_beat_index == 0);
        }
    } else {
        c->pattern_preview_quarter_progress = 0.0;
        c->pattern_preview_beat_index = 0;
    }
    c->was_pattern_record_select = sp303::is_pattern_record_select(dev) && !sp303::is_pattern_recording(dev);

    for (;;) {
        sp303::PatternProjectEvent pev{};
        if (!sp303::consume_pattern_trigger(dev, &pev)) break;
        int slot = pev.sample_pad;
        if (!sp303::pad_has_sample(dev, slot)) continue;
        bool loop_mode = sp303::get_pad_loop_mode(dev, slot);
        bool gate_mode = sp303::get_pad_gate_mode(dev, slot);
        bool reverse_mode = sp303::get_pad_reverse_mode(dev, slot);
        int hold_frames = sp303::audio_get_pad_led_hold_frames(audio, slot, reverse_mode);
        sp303::set_pad_led_hold_frames(dev, slot, hold_frames);
        sp303::note_pattern_pad_played(dev, slot, pev.velocity);
        sp303::audio_trigger_mode_velocity(audio, slot, loop_mode, gate_mode, reverse_mode,
                                           sp303::audio_velocity_gain_from_midi(pev.velocity));
    }
    for (;;) {
        int metro = sp303::consume_pattern_metronome(dev);
        if (metro <= 0) break;
        int level = sp303::get_pattern_metronome_level(dev);
        sp303::audio_trigger_metronome(audio, level, metro == 2);
    }

    bool is_editing_sample = sp303::is_start_end_level_mode(dev);
    if (is_editing_sample && !c->was_editing_sample) {
        int last_played_pad = sp303::get_last_played_pad(dev);
        if (last_played_pad >= 0) {
            float start_knob = sp303::audio_get_sample_start(audio, last_played_pad) / 127.0f;
            float end_knob   = sp303::audio_get_sample_end(audio, last_played_pad) / 127.0f;
            float level_knob = sp303::audio_get_sample_level(audio, last_played_pad) / 127.0f;
            sp303::knob_set(dev, sp303::KNOB_CUTOFF, start_knob);
            sp303::knob_set(dev, sp303::KNOB_RESONANCE, end_knob);
            sp303::knob_set(dev, sp303::KNOB_DRIVE, level_knob);
            state = sp303::get_state(dev);
        }
    }
    if (is_editing_sample) {
        int last_played_pad = sp303::get_last_played_pad(dev);
        if (last_played_pad >= 0) {
            int start_value  = std::clamp((int)std::lround(state.knobs[sp303::KNOB_CUTOFF].value * 127.0f), 0, 127);
            int end_value    = std::clamp((int)std::lround(state.knobs[sp303::KNOB_RESONANCE].value * 127.0f), 0, 127);
            int sample_level = std::clamp((int)std::lround(state.knobs[sp303::KNOB_DRIVE].value * 127.0f), 0, 127);
            sp303::audio_set_sample_start(audio, last_played_pad, start_value);
            sp303::audio_set_sample_end(audio, last_played_pad, end_value);
            sp303::audio_set_sample_level(audio, last_played_pad, sample_level);

            if (active_knob == sp303::KNOB_CUTOFF) {
                sp303::set_edit_display_value(dev, start_value);
            } else if (active_knob == sp303::KNOB_RESONANCE) {
                sp303::set_edit_display_value(dev, end_value);
            } else if (active_knob == sp303::KNOB_DRIVE) {
                sp303::set_edit_display_value(dev, sample_level);
            }
        }
    }
    c->was_editing_sample = is_editing_sample;

    int mark_pad_action = sp303::get_mark_edit_pad(dev);
    int mark_action = sp303::consume_mark_action(dev);
    if (mark_action != 0 && mark_pad_action >= 0 && sp303::pad_has_sample(dev, mark_pad_action)) {
        std::printf("[MARK] consume action=%d bank=%c pad=%d\n",
                    mark_action, "ABCD"[mark_pad_action / 8], (mark_pad_action % 8) + 1);
        if (mark_action == 3) {
            std::printf("[MARK] apply reset full -> start=0 end=127\n");
            sp303::audio_set_sample_start(audio, mark_pad_action, 0);
            sp303::audio_set_sample_end(audio, mark_pad_action, 127);
        } else {
            int playhead = sp303::audio_get_sample_playhead(audio, mark_pad_action);
            std::printf("[MARK] apply playhead=%d\n", playhead);
            if (mark_action == 1) {
                std::printf("[MARK] set START -> %d\n", playhead);
                sp303::audio_set_sample_start(audio, mark_pad_action, playhead);
            } else if (mark_action == 2) {
                std::printf("[MARK] set END -> %d\n", playhead);
                sp303::audio_set_sample_end(audio, mark_pad_action, playhead);
            }
        }
    }

    bool is_time_bpm_mode = sp303::is_time_bpm_mode(dev);
    int time_pad = sp303::get_time_bpm_pad(dev);
    if (is_time_bpm_mode && !c->was_time_bpm_mode && time_pad >= 0 && sp303::pad_has_sample(dev, time_pad)) {
        int sample_bpm = sp303::audio_get_sample_bpm(audio, time_pad);
        int bpm_adjust = sp303::audio_get_sample_bpm_adjust(audio, time_pad);
        int time_mode = sp303::audio_get_sample_time_mode(audio, time_pad);
        int target_bpm = sp303::audio_get_sample_time_target_bpm(audio, time_pad);
        sp303::knob_set(dev, sp303::KNOB_CUTOFF, time_mode_to_knob(time_mode, target_bpm, sample_bpm));
        sp303::knob_set(dev, sp303::KNOB_RESONANCE, bpm_adjust_to_knob(bpm_adjust));
        state = sp303::get_state(dev);
        c->time_bpm_display_kind = (time_mode == 0) ? 0 : ((time_mode == 2) ? 2 : 0);
        c->time_bpm_display_value = (time_mode == 1 && target_bpm >= 40 && target_bpm <= 200) ? target_bpm : sample_bpm;
    }
    if (is_time_bpm_mode && time_pad >= 0 && sp303::pad_has_sample(dev, time_pad)) {
        if (active_knob == sp303::KNOB_RESONANCE) {
            int bpm_adjust = 0;
            if (state.knobs[sp303::KNOB_RESONANCE].value < 0.25f) bpm_adjust = -1;
            else if (state.knobs[sp303::KNOB_RESONANCE].value > 0.75f) bpm_adjust = 1;
            sp303::audio_set_sample_bpm_adjust(audio, time_pad, bpm_adjust);
            c->time_bpm_display_kind = 0;
            c->time_bpm_display_value = sp303::audio_get_sample_bpm(audio, time_pad);
        } else if (active_knob == sp303::KNOB_CUTOFF) {
            float knob = state.knobs[sp303::KNOB_CUTOFF].value;
            int sample_bpm = sp303::audio_get_sample_bpm(audio, time_pad);
            if (knob <= 0.01f) {
                sp303::audio_set_sample_time_mode(audio, time_pad, 0, -1);
                c->time_bpm_display_kind = 1;
            } else if (knob >= 0.99f) {
                sp303::audio_set_sample_time_mode(audio, time_pad, 2, 120);
                c->time_bpm_display_kind = 2;
            } else {
                float ratio = 0.5f + (std::clamp(knob, 0.02f, 0.98f) * 0.8f);
                int target_bpm = std::clamp((int)std::lround(sample_bpm * ratio), 40, 200);
                sp303::audio_set_sample_time_mode(audio, time_pad, 1, target_bpm);
                c->time_bpm_display_kind = 0;
                c->time_bpm_display_value = target_bpm;
            }
        }

        if (c->time_bpm_display_kind == 1) {
            sp303::set_time_bpm_display_off(dev);
        } else if (c->time_bpm_display_kind == 2) {
            sp303::set_time_bpm_display_pattern(dev);
        } else {
            sp303::set_time_bpm_display_number(dev, c->time_bpm_display_value);
        }
    } else if (!is_editing_sample &&
               !mfx_held &&
               !sp303::is_pattern_mode(dev) &&
               !sp303::is_sampling_standby(dev) &&
               !sp303::is_sampling_ready(dev) &&
               !sp303::is_recording(dev) &&
               !sp303::is_threshold_mode(dev) &&
               !sp303::is_delete_mode(dev) &&
               !sp303::is_resampling_mode(dev)) {
        int last_played_pad = sp303::get_last_played_pad(dev);
        if (last_played_pad >= 0 && sp303::pad_has_sample(dev, last_played_pad)) {
            sp303::set_time_bpm_display_number(dev, sp303::audio_get_sample_bpm(audio, last_played_pad));
        }
    }
    c->was_time_bpm_mode = is_time_bpm_mode;

    int mark_pad = sp303::get_last_played_pad(dev);
    bool mark_lit = false;
    if (!sp303::is_pattern_mode(dev) &&
        mark_pad >= 0 &&
        sp303::pad_has_sample(dev, mark_pad)) {
        int start_value = sp303::audio_get_sample_start(audio, mark_pad);
        int end_value   = sp303::audio_get_sample_end(audio, mark_pad);
        mark_lit = (start_value > 0) || (end_value < 127);
    }
    sp303::set_mark_lit(dev, mark_lit);

    int sample_threshold_level = sp303::get_sample_level_threshold(dev);
    float sample_threshold_peak = sample_level_threshold_to_peak(sample_threshold_level);
    bool threshold_crossed = input_peak >= sample_threshold_peak;
    bool was_recording = sp303::audio_is_recording(audio);
    bool is_ready      = sp303::is_sampling_ready(dev);
    bool should_record = sp303::is_recording(dev);
    bool in_standby    = sp303::is_sampling_standby(dev);
    bool resample_mode = sp303::is_resampling_mode(dev);
    bool resample_recording = sp303::is_resample_recording(dev);

    sp303::audio_set_record_from_output(audio, resample_recording);

    if (resample_mode) {
        c->sample_silence_frames = 0;
    } else if (is_ready) {
        if (threshold_crossed) {
            sp303::start_threshold_recording(dev);
            should_record = sp303::is_recording(dev);
            c->sample_silence_frames = 0;
            std::printf("[SAMPLE] trigger crossed: peak=%.4f threshold=%d (%.4f)\n",
                        input_peak, sample_threshold_level, sample_threshold_peak);
        } else {
            c->sample_silence_frames = 0;
        }
    } else if (should_record && !resample_recording) {
        if (threshold_crossed) {
            c->sample_silence_frames = 0;
        } else {
            c->sample_silence_frames++;
            if (c->sample_silence_frames >= 8) {
                std::printf("[SAMPLE] silence stop: peak=%.4f threshold=%d (%.4f)\n",
                            input_peak, sample_threshold_level, sample_threshold_peak);
                sp303::finish_threshold_recording(dev);
                should_record = sp303::is_recording(dev);
                c->sample_silence_frames = 0;
            }
        }
    } else {
        c->sample_silence_frames = 0;
    }

    if (should_record && !was_recording) {
        sp303::audio_start_recording(audio);
    } else if (!should_record && was_recording) {
        sp303::audio_stop_recording(audio);
        int target_pad = sp303::get_last_sampling_target_pad(dev);
        if (target_pad >= 0) {
            bool assigned = sp303::audio_assign_recording(audio, target_pad);
            int bpm_quantize = sp303::consume_record_bpm_quantize(dev);
            if (assigned && bpm_quantize >= 40 && bpm_quantize <= 200) {
                sp303::audio_quantize_sample_end_to_bpm(audio, target_pad, bpm_quantize);
            }
        }
    } else if (!should_record && !in_standby && !resample_mode) {
        sp303::audio_cancel_recording(audio);
    }

    if (sp303::audio_recording_full(audio)) {
        sp303::set_sampling_full(dev);
    }

    int deleted_pad = sp303::consume_deleted_pad(dev);
    if (deleted_pad >= 0) {
        sp303::audio_clear_sample(audio, deleted_pad);
    }
    int truncate_pad = sp303::consume_truncate_pad(dev);
    if (truncate_pad >= 0) {
        sp303::audio_truncate_sample(audio, truncate_pad);
    }
    int delete_all_group = sp303::consume_delete_all_group(dev);
    if (delete_all_group == 0 || delete_all_group == 1) {
        int start = (delete_all_group == 0) ? 0 : 16;
        int end = start + 16;
        for (int i = start; i < end; ++i) {
            sp303::audio_clear_sample(audio, i);
        }
    }
    int swap_a = -1;
    int swap_b = -1;
    if (sp303::consume_swap_pads(dev, &swap_a, &swap_b)) {
        sp303::audio_swap_samples(audio, swap_a, swap_b);
    }
    int card_sample_save_slot = sp303::consume_memory_card_sample_save(dev);
    if (card_sample_save_slot >= 0) {
        memory_card_backup_save_samples(audio, dev, card_sample_save_slot);
    }
    int card_sample_load_slot = sp303::consume_memory_card_sample_load(dev);
    if (card_sample_load_slot >= 0) {
        memory_card_backup_load_samples(audio, dev, card_sample_load_slot);
    }
    if (sp303::consume_memory_card_dirty(dev)) {
        CardIoResult res = card_save(c->card_path, dev, audio, c->audio_cfg);
        if (!res.ok) {
            std::printf("[CARD] %s\n", res.message.c_str());
        }
    }

    if (!sp303::is_start_end_level_mode(dev) &&
        !sp303::is_time_bpm_mode(dev) &&
        !knob_mode_owns_input_gain(dev) &&
        !mfx_held) {
        int active_fx  = sp303::get_active_effect_btn(dev);
        int mfx_sub    = sp303::get_mfx_type(dev);
        int def_idx    = sp303::fx_btn_to_index(active_fx, mfx_sub);
        if (def_idx >= 0 && def_idx < sp303::FX_SLOT_COUNT) {
            const sp303::EffectDef& def = sp303::FX_DEFS[def_idx];
            const char* label = nullptr;
            if      (active_knob == sp303::KNOB_CUTOFF)    label = def.p1_label;
            else if (active_knob == sp303::KNOB_RESONANCE) label = def.p2_label;
            else if (active_knob == sp303::KNOB_DRIVE)     label = def.p3_label;

            // Delay p1: show note value instead of static label
            if (active_fx == sp303::BTN_DELAY && active_knob == sp303::KNOB_CUTOFF) {
                uint8_t segs[3];
                sp303::delay_note_display(c->cached_fx_p1, segs);
                sp303::display_raw(dev, segs[0], segs[1], segs[2]);
            } else if (label) {
                sp303::display_str3(dev, label);
            }
        }
    }

    return sp303::get_state(dev);
}
