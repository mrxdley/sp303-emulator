#include "controller.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static const float DEFAULT_PEAK_THRESHOLD = 0.05f;
static const uint32_t SAMPLE_RATES[] = {44100, 48000, 96000};
static const uint32_t BUFFER_SIZES[] = {128, 256, 512, 1024, 2048};
static const int N_RATES = 3;
static const int N_BUFS = 5;
static const char* AUDIO_CFG_FILE = "audio_config.json";

struct AppConfig {
    SP303AudioConfig audio{};
    float peak_threshold = DEFAULT_PEAK_THRESHOLD;
};

static float sample_level_threshold_to_peak(int level) {
    if (level <= 0) return 0.003f;
    return std::clamp(level * 0.01f, 0.0f, 1.0f);
}

static SP303AudioQuality to_audio_quality(SP303SampleQuality q) {
    switch (q) {
        case SP303_SAMPLE_QUALITY_LONG: return SP303_AUDIO_QUALITY_LONG;
        case SP303_SAMPLE_QUALITY_LOFI: return SP303_AUDIO_QUALITY_LOFI;
        case SP303_SAMPLE_QUALITY_STANDARD:
        default: return SP303_AUDIO_QUALITY_STANDARD;
    }
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
    sp303_audio_config_default(&cfg.audio);

    if (virtual_input_exists()) {
        std::strncpy(cfg.audio.input_name, "Monitor of SP303_Input", SP303_AUDIO_NAME_LEN - 1);
    }

    std::ifstream f(AUDIO_CFG_FILE);
    if (!f.is_open()) return cfg;
    try {
        auto j = json::parse(f);
        if (j.contains("output")) std::strncpy(cfg.audio.output_name, j["output"].get<std::string>().c_str(), SP303_AUDIO_NAME_LEN - 1);
        if (j.contains("input") && !virtual_input_exists()) {
            std::strncpy(cfg.audio.input_name, j["input"].get<std::string>().c_str(), SP303_AUDIO_NAME_LEN - 1);
        }
        if (j.contains("rate"))   cfg.audio.sample_rate   = j["rate"];
        if (j.contains("buffer")) cfg.audio.buffer_frames = j["buffer"];
        if (j.contains("peak_threshold")) {
            cfg.peak_threshold = std::clamp(j["peak_threshold"].get<float>(), 0.0f, 1.0f);
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
    std::ofstream f(AUDIO_CFG_FILE);
    f << j.dump(2);
}

void renderer_controller_refresh_devices(RendererController* c) {
    if (!c) return;
    c->out_devs.clear();
    c->in_devs.clear();
    if (!c->audio) return;

    c->out_devs.resize(64);
    c->in_devs.resize(64);
    int n_out = sp303_audio_list_outputs(c->audio, c->out_devs.data(), 64);
    int n_in  = sp303_audio_list_inputs(c->audio, c->in_devs.data(), 64);
    c->out_devs.resize(n_out);
    c->in_devs.resize(n_in);
}

bool renderer_controller_init(RendererController* c) {
    if (!c) return false;
    AppConfig cfg = load_app_config();
    c->audio_cfg = cfg.audio;
    c->peak_threshold = cfg.peak_threshold;
    c->audio = sp303_audio_create(&c->audio_cfg);
    c->playback_ok = (c->audio != nullptr);
    renderer_controller_refresh_devices(c);

    for (int i = 0; i < (int)c->out_devs.size(); ++i) {
        if (std::strncmp(c->out_devs[i].name, c->audio_cfg.output_name, SP303_AUDIO_NAME_LEN) == 0) {
            c->sel_out = i;
            break;
        }
    }
    for (int i = 0; i < (int)c->in_devs.size(); ++i) {
        if (std::strncmp(c->in_devs[i].name, c->audio_cfg.input_name, SP303_AUDIO_NAME_LEN) == 0) {
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
    return true;
}

void renderer_controller_shutdown(RendererController* c) {
    if (!c) return;
    sp303_audio_destroy(c->audio);
    c->audio = nullptr;
}

bool renderer_controller_apply_audio_config(RendererController* c) {
    if (!c) return false;
    SP303AudioConfig new_cfg = c->audio_cfg;
    if (!c->out_devs.empty()) {
        std::strncpy(new_cfg.output_name, c->out_devs[c->sel_out].name, SP303_AUDIO_NAME_LEN - 1);
    }
    if (!c->in_devs.empty()) {
        std::strncpy(new_cfg.input_name, c->in_devs[c->sel_in].name, SP303_AUDIO_NAME_LEN - 1);
    }
    new_cfg.sample_rate   = SAMPLE_RATES[c->sel_rate];
    new_cfg.buffer_frames = BUFFER_SIZES[c->sel_buf];

    bool ok = true;
    if (c->audio) {
        ok = sp303_audio_reconfigure(c->audio, &new_cfg);
    }
    c->playback_ok = ok;
    if (ok) {
        c->audio_cfg = new_cfg;
        save_app_config(c);
        renderer_controller_refresh_devices(c);
    }
    return ok;
}

SP303State renderer_controller_step(RendererController* c, SP303Device* dev, int active_knob) {
    sp303_tick(dev, 735);
    SP303State state = sp303_get_state(dev);

    if (!c || !c->audio) return state;

    SP303Audio* audio = c->audio;
    sp303_audio_set_recording_mode(audio,
                                   sp303_get_sampling_stereo(dev),
                                   to_audio_quality(sp303_get_sampling_quality(dev)));
    float input_peak = sp303_audio_input_peak(audio);
    float output_peak = sp303_audio_peak(audio);
    float stereo_diff_peak = sp303_audio_stereo_diff_peak(audio);
    float global_peak = std::max(input_peak, output_peak);
    bool should_light_peak = global_peak > c->peak_threshold;
    c->stereo_activity_lit = (output_peak > 0.01f) && (stereo_diff_peak > 0.01f);

    if (should_light_peak) {
        c->peak_hold_frames = 10;
    }
    if (c->peak_hold_frames > 0) {
        c->peak_hold_frames--;
        sp303_indicator_set(dev, SP303_IND_PEAK, true);
    } else {
        sp303_indicator_set(dev, SP303_IND_PEAK, false);
    }
    c->config_input_peak = input_peak;

    float input_gain = state.knobs[SP303_KNOB_DRIVE].value;
    sp303_audio_set_input_gain(audio, input_gain);
    sp303_audio_set_output_gain(audio, state.knobs[SP303_KNOB_VOLUME].value);

    bool is_editing_sample = sp303_is_start_end_level_mode(dev);
    if (is_editing_sample && !c->was_editing_sample) {
        int last_played_pad = sp303_get_last_played_pad(dev);
        if (last_played_pad >= 0) {
            float start_knob = sp303_audio_get_sample_start(audio, last_played_pad) / 127.0f;
            float end_knob = sp303_audio_get_sample_end(audio, last_played_pad) / 127.0f;
            float level_knob = sp303_audio_get_sample_level(audio, last_played_pad) / 127.0f;
            sp303_knob_set(dev, SP303_KNOB_CUTOFF, start_knob);
            sp303_knob_set(dev, SP303_KNOB_RESONANCE, end_knob);
            sp303_knob_set(dev, SP303_KNOB_DRIVE, level_knob);
            state = sp303_get_state(dev);
        }
    }
    if (is_editing_sample) {
        int last_played_pad = sp303_get_last_played_pad(dev);
        if (last_played_pad >= 0) {
            int start_value = std::clamp((int)std::lround(state.knobs[SP303_KNOB_CUTOFF].value * 127.0f), 0, 127);
            int end_value = std::clamp((int)std::lround(state.knobs[SP303_KNOB_RESONANCE].value * 127.0f), 0, 127);
            int sample_level = std::clamp((int)std::lround(state.knobs[SP303_KNOB_DRIVE].value * 127.0f), 0, 127);
            sp303_audio_set_sample_start(audio, last_played_pad, start_value);
            sp303_audio_set_sample_end(audio, last_played_pad, end_value);
            sp303_audio_set_sample_level(audio, last_played_pad, sample_level);

            if (active_knob == SP303_KNOB_CUTOFF) {
                sp303_set_edit_display_value(dev, start_value);
            } else if (active_knob == SP303_KNOB_RESONANCE) {
                sp303_set_edit_display_value(dev, end_value);
            } else if (active_knob == SP303_KNOB_DRIVE) {
                sp303_set_edit_display_value(dev, sample_level);
            }
        }
    }
    c->was_editing_sample = is_editing_sample;

    int mark_pad = sp303_get_last_played_pad(dev);
    bool mark_lit = false;
    if (mark_pad >= 0 && sp303_pad_has_sample(dev, mark_pad)) {
        int start_value = sp303_audio_get_sample_start(audio, mark_pad);
        int end_value = sp303_audio_get_sample_end(audio, mark_pad);
        mark_lit = (start_value > 0) || (end_value < 127);
    }
    sp303_set_mark_lit(dev, mark_lit);

    int sample_threshold_level = sp303_get_sample_level_threshold(dev);
    float sample_threshold_peak = sample_level_threshold_to_peak(sample_threshold_level);
    bool threshold_crossed = input_peak >= sample_threshold_peak;
    bool was_recording = sp303_audio_is_recording(audio);
    bool is_ready = sp303_is_sampling_ready(dev);
    bool should_record = sp303_is_recording(dev);
    bool in_standby = sp303_is_sampling_standby(dev);

    if (is_ready) {
        if (threshold_crossed) {
            sp303_start_threshold_recording(dev);
            should_record = sp303_is_recording(dev);
            c->sample_silence_frames = 0;
            std::printf("[SAMPLE] trigger crossed: peak=%.4f threshold=%d (%.4f)\n",
                        input_peak, sample_threshold_level, sample_threshold_peak);
        } else {
            c->sample_silence_frames = 0;
        }
    } else if (should_record) {
        if (threshold_crossed) {
            c->sample_silence_frames = 0;
        } else {
            c->sample_silence_frames++;
            if (c->sample_silence_frames >= 8) {
                std::printf("[SAMPLE] silence stop: peak=%.4f threshold=%d (%.4f)\n",
                            input_peak, sample_threshold_level, sample_threshold_peak);
                sp303_finish_threshold_recording(dev);
                should_record = sp303_is_recording(dev);
                c->sample_silence_frames = 0;
            }
        }
    } else {
        c->sample_silence_frames = 0;
    }

    if (should_record && !was_recording) {
        sp303_audio_start_recording(audio);
    } else if (!should_record && was_recording) {
        sp303_audio_stop_recording(audio);
        int target_pad = sp303_get_last_sampling_target_pad(dev);
        if (target_pad >= 0) {
            bool assigned = sp303_audio_assign_recording(audio, target_pad);
            int bpm_quantize = sp303_consume_record_bpm_quantize(dev);
            if (assigned && bpm_quantize >= 40 && bpm_quantize <= 200) {
                sp303_audio_quantize_sample_end_to_bpm(audio, target_pad, bpm_quantize);
            }
        }
    } else if (!should_record && !in_standby) {
        sp303_audio_cancel_recording(audio);
    }

    if (sp303_audio_recording_full(audio)) {
        sp303_set_sampling_full(dev);
    }

    int deleted_pad = sp303_consume_deleted_pad(dev);
    if (deleted_pad >= 0) {
        sp303_audio_clear_sample(audio, deleted_pad);
    }
    int truncate_pad = sp303_consume_truncate_pad(dev);
    if (truncate_pad >= 0) {
        sp303_audio_truncate_sample(audio, truncate_pad);
    }
    int delete_all_group = sp303_consume_delete_all_group(dev);
    if (delete_all_group == 0 || delete_all_group == 1) {
        int start = (delete_all_group == 0) ? 0 : 16;
        int end = start + 16;
        for (int i = start; i < end; ++i) {
            sp303_audio_clear_sample(audio, i);
        }
    }
    int swap_a = -1;
    int swap_b = -1;
    if (sp303_consume_swap_pads(dev, &swap_a, &swap_b)) {
        sp303_audio_swap_samples(audio, swap_a, swap_b);
    }

    return sp303_get_state(dev);
}
