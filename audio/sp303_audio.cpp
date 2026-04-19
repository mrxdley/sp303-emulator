#define MINIAUDIO_IMPLEMENTATION
#include "sp303_audio_internal.h"
#include "sample_ops.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <new>

// ─── Config ───────────────────────────────────────────────────────────────────

void sp303_audio_config_default(SP303AudioConfig* cfg) {
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->sample_rate   = 44100;
    cfg->buffer_frames = 512;
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

static void playback_cb(ma_device* dev, void* out_raw, const void*, ma_uint32 frames) {
    SP303Audio* a = static_cast<SP303Audio*>(dev->pUserData);
    float* out    = static_cast<float*>(out_raw);
    const uint32_t n = frames * 2; // stereo output

    std::memset(out, 0, n * sizeof(float));

    float peak = 0.0f;
    float stereo_diff_peak = 0.0f;
    {
        std::lock_guard<std::mutex> lock(a->voice_mutex);
        for (auto& v : a->voices) {
            if (!v.active || v.slot < 0) continue;
            const auto& s = a->samples[v.slot];
            if (s.pcm.empty()) { v.active = false; continue; }
            if (v.end_position == 0 || v.end_position > s.pcm.size())
                v.end_position = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));

            for (uint32_t f = 0; f < frames; ++f) {
                if (v.position >= v.end_position) {
                    if (v.looping) {
                        v.position = v.loop_start;
                    } else {
                        v.active = false;
                        break;
                    }
                }
                if (s.channels == 2) {
                    uint32_t idx = v.position * 2;
                    float l = s.pcm[idx] * (v.gain * s.level);
                    float r = s.pcm[idx + 1] * (v.gain * s.level);
                    out[f * 2]     += l;
                    out[f * 2 + 1] += r;
                    v.position++;
                } else {
                    float samp = s.pcm[v.position++] * (v.gain * s.level);
                    out[f * 2]     += samp;
                    out[f * 2 + 1] += samp;
                }
            }
        }
    }

    const float gain = a->output_gain.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < n; ++i) {
        out[i] *= gain;
        peak = std::max(peak, std::abs(out[i]));
    }
    for (uint32_t f = 0; f < frames; ++f) {
        float l = out[f * 2];
        float r = out[f * 2 + 1];
        stereo_diff_peak = std::max(stereo_diff_peak, std::abs(l - r));
    }

    a->output_peak.store(peak, std::memory_order_relaxed);
    a->stereo_diff_peak.store(stereo_diff_peak, std::memory_order_relaxed);
}

static void capture_cb(ma_device* dev, void* out_raw, const void* in_raw, ma_uint32 frames) {
    (void)out_raw; // capture only
    SP303Audio* a = static_cast<SP303Audio*>(dev->pUserData);
    const float* in = static_cast<const float*>(in_raw);

    // Calculate input peak (stereo -> mono mix)
    float peak = 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        // Average left and right channels
        float s = (std::abs(in[i * 2]) + std::abs(in[i * 2 + 1])) * 0.5f;
        peak = std::max(peak, s);
        sum += s;
    }
    float avg = frames > 0 ? sum / frames : 0.0f;
    a->input_peak.store(peak, std::memory_order_relaxed);

    // Debug: print capture stats periodically
    static int dbg_cap = 0;
    if (++dbg_cap >= 200) { // ~3 seconds at 60fps
        dbg_cap = 0;
        std::printf("[CAP] frames=%u peak=%.4f avg=%.4f rec_active=%d\n",
                    frames, peak, avg, a->rec_active ? 1 : 0);
    }

    // Recording
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    if (!a->rec_active || a->rec_buffer.empty()) return;

    float gain = a->input_gain.load(std::memory_order_relaxed);
    uint32_t frames_written = 0;

    for (uint32_t i = 0; i < frames; ++i) {
        if (a->rec_frames >= a->rec_max_frames) {
            a->rec_full = true;
            a->rec_active = false;
            break;
        }
        if (a->rec_stereo) {
            uint32_t idx = a->rec_write_pos * 2;
            a->rec_buffer[idx]     = in[i * 2] * gain;
            a->rec_buffer[idx + 1] = in[i * 2 + 1] * gain;
        } else {
            float samp = ((in[i * 2] + in[i * 2 + 1]) * 0.5f) * gain;
            a->rec_buffer[a->rec_write_pos] = samp;
        }
        a->rec_write_pos = (a->rec_write_pos + 1) % a->rec_max_frames;
        a->rec_frames++;
        frames_written++;
    }

    // Debug: print recording stats every second or so (roughly)
    static int dbg_count = 0;
    if (++dbg_count >= 100) { // ~1.6 seconds at 60fps
        dbg_count = 0;
        std::printf("[REC] frames_written=%u total=%u peak=%.3f gain=%.2f\n",
                    frames_written, a->rec_frames, peak, gain);
    }
}

// ─── Device enumeration ───────────────────────────────────────────────────────

static void cache_devices(SP303Audio* a) {
    ma_device_info* p_out;  ma_uint32 n_out;
    ma_device_info* p_in;   ma_uint32 n_in;
    if (ma_context_get_devices(&a->context, &p_out, &n_out, &p_in, &n_in) != MA_SUCCESS)
        return;
    a->out_devs.assign(p_out, p_out + n_out);
    a->in_devs .assign(p_in,  p_in  + n_in);
}

// ─── Open / close playback device ────────────────────────────────────────────

static bool open_playback(SP303Audio* a) {
    ma_device_config dcfg      = ma_device_config_init(ma_device_type_playback);
    dcfg.playback.format       = ma_format_f32;
    dcfg.playback.channels     = 2;
    dcfg.sampleRate            = a->cfg.sample_rate;
    dcfg.periodSizeInFrames    = a->cfg.buffer_frames;
    dcfg.dataCallback          = playback_cb;
    dcfg.pUserData             = a;

    // Match device by name if one is specified
    if (a->cfg.output_name[0] != '\0') {
        for (auto& d : a->out_devs) {
            if (std::strncmp(d.name, a->cfg.output_name, sizeof(d.name) - 1) == 0) {
                dcfg.playback.pDeviceID = &d.id;
                break;
            }
        }
    }

    if (ma_device_init(&a->context, &dcfg, &a->playback) != MA_SUCCESS)
        return false;
    if (ma_device_start(&a->playback) != MA_SUCCESS) {
        ma_device_uninit(&a->playback);
        return false;
    }
    return true;
}

static bool open_capture(SP303Audio* a) {
    ma_device_config dcfg = ma_device_config_init(ma_device_type_capture);
    dcfg.capture.format = ma_format_f32;
    dcfg.capture.channels = 2; // stereo input (virtual sink provides stereo)
    dcfg.sampleRate = a->cfg.sample_rate;
    dcfg.periodSizeInFrames = a->cfg.buffer_frames;
    dcfg.dataCallback = capture_cb;
    dcfg.pUserData = a;

    // Match device by name if one is specified
    std::printf("[AUDIO] Opening capture device, requested: '%s'\n", a->cfg.input_name);
    if (a->cfg.input_name[0] != '\0') {
        for (auto& d : a->in_devs) {
            std::printf("[AUDIO]   Checking: '%s'\n", d.name);
            if (std::strncmp(d.name, a->cfg.input_name, sizeof(d.name) - 1) == 0) {
                dcfg.capture.pDeviceID = &d.id;
                std::printf("[AUDIO]   -> Matched!\n");
                break;
            }
        }
    }

    if (ma_device_init(&a->context, &dcfg, &a->capture) != MA_SUCCESS) {
        std::printf("[AUDIO] Capture device init FAILED\n");
        return false;
    }
    if (ma_device_start(&a->capture) != MA_SUCCESS) {
        std::printf("[AUDIO] Capture device start FAILED\n");
        ma_device_uninit(&a->capture);
        return false;
    }
    std::printf("[AUDIO] Capture device opened successfully\n");
    return true;
}

static void close_playback(SP303Audio* a) {
    if (a->playback_ok) {
        ma_device_uninit(&a->playback);
        a->playback_ok = false;
    }
}

static void close_capture(SP303Audio* a) {
    if (a->capture_ok) {
        ma_device_uninit(&a->capture);
        a->capture_ok = false;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

SP303Audio* sp303_audio_create(const SP303AudioConfig* cfg) {
    SP303Audio* a = new (std::nothrow) SP303Audio();
    if (!a) return nullptr;

    if (cfg) a->cfg = *cfg;
    else     sp303_audio_config_default(&a->cfg);

    if (ma_context_init(nullptr, 0, nullptr, &a->context) != MA_SUCCESS) {
        delete a;
        return nullptr;
    }
    a->context_ok = true;
    cache_devices(a);

    a->playback_ok = open_playback(a);
    a->capture_ok = open_capture(a);

    // Pre-allocate recording buffer (60 seconds at current sample rate)
    a->rec_buffer.resize(SP303_AUDIO_MAX_FRAMES);

    return a;
}

void sp303_audio_destroy(SP303Audio* a) {
    if (!a) return;
    close_capture(a);
    close_playback(a);
    if (a->context_ok) ma_context_uninit(&a->context);
    delete a;
}

// ─── Samples ──────────────────────────────────────────────────────────────────

void sp303_audio_load_sample(SP303Audio* a, int slot, const float* pcm, uint32_t frames) {
    sp303_audio_load_sample_impl(a, slot, pcm, frames);
}

void sp303_audio_clear_sample(SP303Audio* a, int slot) {
    sp303_audio_clear_sample_impl(a, slot);
}

void sp303_audio_swap_samples(SP303Audio* a, int slot_a, int slot_b) {
    sp303_audio_swap_samples_impl(a, slot_a, slot_b);
}

void sp303_audio_set_sample_start(SP303Audio* a, int slot, int value) {
    sp303_audio_set_sample_start_impl(a, slot, value);
}

void sp303_audio_set_sample_end(SP303Audio* a, int slot, int value) {
    sp303_audio_set_sample_end_impl(a, slot, value);
}

int sp303_audio_get_sample_start(SP303Audio* a, int slot) {
    return sp303_audio_get_sample_start_impl(a, slot);
}

int sp303_audio_get_sample_end(SP303Audio* a, int slot) {
    return sp303_audio_get_sample_end_impl(a, slot);
}

bool sp303_audio_truncate_sample(SP303Audio* a, int slot) {
    return sp303_audio_truncate_sample_impl(a, slot);
}

bool sp303_audio_quantize_sample_end_to_bpm(SP303Audio* a, int slot, int bpm) {
    return sp303_audio_quantize_sample_end_to_bpm_impl(a, slot, bpm);
}

void sp303_audio_set_sample_level(SP303Audio* a, int slot, int level) {
    sp303_audio_set_sample_level_impl(a, slot, level);
}

int sp303_audio_get_sample_level(SP303Audio* a, int slot) {
    return sp303_audio_get_sample_level_impl(a, slot);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

bool sp303_audio_is_playing(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    for (const auto& v : a->voices) {
        if (v.active && v.slot == slot) return true;
    }
    return false;
}

void sp303_audio_note_off(SP303Audio* a, int slot) {
    sp303_audio_stop(a, slot);
}

void sp303_audio_trigger_mode(SP303Audio* a, int slot, bool loop, bool gate) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    if (s.pcm.empty()) return;

    const int bank = slot / 8;
    const int pad = (slot % 8) + 1;
    const char bank_name = static_cast<char>('A' + bank);
    const uint32_t sample_size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    const uint32_t start = std::min(s.start_frame, sample_size - 1);
    const uint32_t end = std::clamp((s.end_frame == 0 ? sample_size : s.end_frame), start + 1, sample_size);

    bool playing = false;
    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) {
            playing = true;
            break;
        }
    }

    // Trigger + Loop: pressing the pad while playing stops it.
    if (!gate && loop && playing) {
        for (auto& v : a->voices) {
            if (v.active && v.slot == slot) v.active = false;
        }
        std::printf("[PAD] Stopped bank %c pad %d (slot %d, trigger-loop toggle)\n",
                    bank_name, pad, slot);
        return;
    }

    // Retrigger existing voice for this slot if present.
    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) {
            v.position = start;
            v.loop_start = start;
            v.end_position = end;
            v.looping = loop;
            v.gain = 1.0f;
            std::printf("[PAD] Played bank %c pad %d (slot %d, retrigger)\n",
                        bank_name, pad, slot);
            return;
        }
    }

    Voice* target = nullptr;
    for (auto& v : a->voices) {
        if (!v.active) { target = &v; break; }
    }
    if (!target) {
        uint32_t max_pos = 0;
        for (auto& v : a->voices) {
            if (v.position > max_pos) { max_pos = v.position; target = &v; }
        }
    }
    if (!target) target = &a->voices[0];

    target->slot         = slot;
    target->position     = start;
    target->loop_start   = start;
    target->end_position = end;
    target->gain         = 1.0f;
    target->looping      = loop;
    target->active       = true;
    std::printf("[PAD] Played bank %c pad %d (slot %d)%s%s\n",
                bank_name, pad, slot,
                loop ? " [LOOP]" : "",
                gate ? " [GATE]" : "");
}

void sp303_audio_trigger(SP303Audio* a, int slot) {
    sp303_audio_trigger_mode(a, slot, false, false);
}

void sp303_audio_stop(SP303Audio* a, int slot) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    for (auto& v : a->voices)
        if (v.active && v.slot == slot) v.active = false;
}

// ─── Metering ─────────────────────────────────────────────────────────────────

float sp303_audio_peak(SP303Audio* a) {
    if (!a) return 0.0f;
    return a->output_peak.load(std::memory_order_relaxed);
}

float sp303_audio_stereo_diff_peak(SP303Audio* a) {
    if (!a) return 0.0f;
    return a->stereo_diff_peak.load(std::memory_order_relaxed);
}

void sp303_audio_set_output_gain(SP303Audio* a, float gain) {
    if (!a) return;
    a->output_gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
}

float sp303_audio_input_peak(SP303Audio* a) {
    if (!a) return 0.0f;
    return a->input_peak.load(std::memory_order_relaxed);
}

void sp303_audio_set_input_gain(SP303Audio* a, float gain) {
    if (!a) return;
    a->input_gain.store(std::clamp(gain, 0.0f, 2.0f), std::memory_order_relaxed);
}

// ─── Device enumeration ───────────────────────────────────────────────────────

int sp303_audio_list_outputs(SP303Audio* a, SP303AudioDeviceInfo* out, int max) {
    if (!a || !out || max <= 0) return 0;
    int n = std::min((int)a->out_devs.size(), max);
    for (int i = 0; i < n; ++i) {
        std::strncpy(out[i].name, a->out_devs[i].name, SP303_AUDIO_NAME_LEN - 1);
        out[i].name[SP303_AUDIO_NAME_LEN - 1] = '\0';
        out[i].is_default = (a->out_devs[i].isDefault != 0);
    }
    return n;
}

int sp303_audio_list_inputs(SP303Audio* a, SP303AudioDeviceInfo* out, int max) {
    if (!a || !out || max <= 0) return 0;
    int n = std::min((int)a->in_devs.size(), max);
    for (int i = 0; i < n; ++i) {
        std::strncpy(out[i].name, a->in_devs[i].name, SP303_AUDIO_NAME_LEN - 1);
        out[i].name[SP303_AUDIO_NAME_LEN - 1] = '\0';
        out[i].is_default = (a->in_devs[i].isDefault != 0);
    }
    return n;
}

// ─── Recording ────────────────────────────────────────────────────────────────

void sp303_audio_start_recording(SP303Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    uint32_t multiplier = 1;
    if (a->rec_quality == SP303_AUDIO_QUALITY_LONG) multiplier = 2;
    else if (a->rec_quality == SP303_AUDIO_QUALITY_LOFI) multiplier = 4;
    a->rec_max_frames = SP303_AUDIO_MAX_FRAMES * multiplier;
    a->rec_buffer.resize(a->rec_stereo ? (a->rec_max_frames * 2) : a->rec_max_frames);
    a->rec_write_pos = 0;
    a->rec_frames = 0;
    a->rec_full = false;
    a->rec_active = true;
}

void sp303_audio_stop_recording(SP303Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_active = false;
}

bool sp303_audio_is_recording(SP303Audio* a) {
    if (!a) return false;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    return a->rec_active;
}

bool sp303_audio_recording_full(SP303Audio* a) {
    if (!a) return false;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    return a->rec_full;
}

bool sp303_audio_assign_recording(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->rec_mutex);

    if (a->rec_frames == 0 || a->rec_buffer.empty()) return false;

    // Copy recorded frames from the ring buffer
    std::vector<float> recorded;
    recorded.reserve(a->rec_frames * (a->rec_stereo ? 2 : 1));

    // Ring buffer: if we didn't wrap around, data is at [0..rec_frames)
    // If we wrapped, data is spread across the buffer
    uint32_t start_frame = (a->rec_write_pos >= a->rec_frames)
        ? (a->rec_write_pos - a->rec_frames)
        : (a->rec_max_frames + a->rec_write_pos - a->rec_frames);
    for (uint32_t i = 0; i < a->rec_frames; ++i) {
        uint32_t frame = (start_frame + i) % a->rec_max_frames;
        if (a->rec_stereo) {
            recorded.push_back(a->rec_buffer[frame * 2]);
            recorded.push_back(a->rec_buffer[frame * 2 + 1]);
        } else {
            recorded.push_back(a->rec_buffer[frame]);
        }
    }

    auto quantize = [](float x, int bits) -> float {
        float levels = (float)((1 << bits) - 1);
        float v = std::clamp((x + 1.0f) * 0.5f, 0.0f, 1.0f);
        float q = std::round(v * levels) / levels;
        return (q * 2.0f) - 1.0f;
    };
    if (a->rec_quality == SP303_AUDIO_QUALITY_LONG || a->rec_quality == SP303_AUDIO_QUALITY_LOFI) {
        int channels = a->rec_stereo ? 2 : 1;
        float alpha = (a->rec_quality == SP303_AUDIO_QUALITY_LONG) ? 0.20f : 0.08f;
        int bits = (a->rec_quality == SP303_AUDIO_QUALITY_LONG) ? 12 : 8;
        std::vector<float> prev(channels, 0.0f);
        for (size_t i = 0; i < recorded.size(); ++i) {
            int ch = (int)(i % channels);
            float lp = prev[ch] + alpha * (recorded[i] - prev[ch]);
            prev[ch] = lp;
            recorded[i] = quantize(lp, bits);
        }
    }

    // Debug: check the sample data
    float min_val = 0.0f, max_val = 0.0f, sum = 0.0f;
    for (float s : recorded) {
        min_val = std::min(min_val, s);
        max_val = std::max(max_val, s);
        sum += std::abs(s);
    }
    float avg = recorded.empty() ? 0.0f : sum / recorded.size();
    std::printf("[ASSIGN] slot=%d frames=%zu min=%.4f max=%.4f avg=%.4f\n",
                slot, recorded.size(), min_val, max_val, avg);

    // Assign to the slot (under voice_mutex)
    {
        std::lock_guard<std::mutex> vlock(a->voice_mutex);
        a->samples[slot].pcm = std::move(recorded);
        a->samples[slot].channels = a->rec_stereo ? 2u : 1u;
        a->samples[slot].level = 1.0f;
        a->samples[slot].start_frame = 0;
        a->samples[slot].end_frame = (uint32_t)(a->samples[slot].pcm.size() / a->samples[slot].channels);
    }

    // Clear recording state
    a->rec_frames = 0;
    a->rec_write_pos = 0;
    a->rec_full = false;

    return true;
}

void sp303_audio_cancel_recording(SP303Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_active = false;
    a->rec_frames = 0;
    a->rec_write_pos = 0;
    a->rec_full = false;
}

void sp303_audio_set_recording_mode(SP303Audio* a, bool stereo, SP303AudioQuality quality) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_stereo = stereo;
    a->rec_quality = quality;
}

// ─── Reconfigure ──────────────────────────────────────────────────────────────

bool sp303_audio_reconfigure(SP303Audio* a, const SP303AudioConfig* cfg) {
    if (!a || !cfg) return false;
    close_capture(a);
    close_playback(a);
    a->cfg = *cfg;
    a->playback_ok = open_playback(a);
    a->capture_ok = open_capture(a);
    // Resize recording buffer for new sample rate
    a->rec_buffer.resize(SP303_AUDIO_MAX_FRAMES);
    return a->playback_ok && a->capture_ok;
}
