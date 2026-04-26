#define MINIAUDIO_IMPLEMENTATION
#include "sp303_audio_internal.h"
#include "sample_ops.h"
#include "sp303.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <new>

namespace sp303 {

static constexpr int METRONOME_MS = 35;
static constexpr float BUS_EFFECT_MAX_SEC = 2.0f;

static inline float read_sample_linear(const Sample& s, float frame_pos, uint32_t channel) {
    const uint32_t ch = std::min(channel, std::max(1u, s.channels) - 1u);
    const uint32_t frame_count = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (frame_count == 0) return 0.0f;
    frame_pos = std::clamp(frame_pos, 0.0f, std::max(0.0f, (float)frame_count - 1.0f));
    const uint32_t i0 = (uint32_t)frame_pos;
    const uint32_t i1 = std::min(i0 + 1, frame_count - 1);
    const float frac = frame_pos - (float)i0;
    if (s.channels == 1) {
        const float a = s.pcm[i0];
        const float b = s.pcm[i1];
        return a + (b - a) * frac;
    }
    const float a = s.pcm[i0 * s.channels + ch];
    const float b = s.pcm[i1 * s.channels + ch];
    return a + (b - a) * frac;
}

static inline float read_pcm_linear(const std::vector<float>& pcm, uint32_t channels, float frame_pos, uint32_t channel) {
    channels = std::max(1u, channels);
    const uint32_t frame_count = (uint32_t)(pcm.size() / channels);
    if (frame_count == 0) return 0.0f;
    const uint32_t ch = std::min(channel, channels - 1);
    frame_pos = std::clamp(frame_pos, 0.0f, std::max(0.0f, (float)frame_count - 1.0f));
    const uint32_t i0 = (uint32_t)frame_pos;
    const uint32_t i1 = std::min(i0 + 1, frame_count - 1);
    const float frac = frame_pos - (float)i0;
    const float a = pcm[i0 * channels + ch];
    const float b = pcm[i1 * channels + ch];
    return a + (b - a) * frac;
}

static void render_time_stretch(const Sample& s,
                                uint32_t start_frame,
                                uint32_t end_frame,
                                float stretch_ratio,
                                uint32_t sample_rate,
                                std::vector<float>* out_pcm,
                                uint32_t* out_frames) {
    if (!out_pcm || !out_frames) return;
    out_pcm->clear();
    *out_frames = 0;

    const uint32_t channels = std::max(1u, s.channels);
    const uint32_t in_frames = (end_frame > start_frame) ? (end_frame - start_frame) : 0;
    if (in_frames < 2) return;

    stretch_ratio = std::clamp(stretch_ratio, 0.5f, 2.0f);
    const uint32_t grain = std::clamp(sample_rate / 50, 256u, 2048u);
    const uint32_t overlap = std::max(64u, grain / 4);
    const uint32_t synth_hop = std::max(1u, grain - overlap);
    const uint32_t analysis_hop = std::max(1u, (uint32_t)std::lround(synth_hop / stretch_ratio));
    const uint32_t out_len = std::max(grain, (uint32_t)std::lround(in_frames * stretch_ratio) + grain);

    out_pcm->assign(out_len * channels, 0.0f);
    std::vector<float> norm(out_len, 0.0f);

    for (uint32_t in_pos = 0, out_pos = 0; in_pos < in_frames; in_pos += analysis_hop, out_pos += synth_hop) {
        for (uint32_t g = 0; g < grain; ++g) {
            const uint32_t out_frame = out_pos + g;
            if (out_frame >= out_len) break;
            const float win = 0.5f - 0.5f * std::cos((2.0f * 3.1415926535f * g) / std::max(1u, grain - 1));
            const float src_frame = (float)start_frame + std::min<float>(in_pos + g, (float)(in_frames - 1));
            for (uint32_t ch = 0; ch < channels; ++ch) {
                (*out_pcm)[out_frame * channels + ch] += read_sample_linear(s, src_frame, ch) * win;
            }
            norm[out_frame] += win;
        }
    }

    for (uint32_t f = 0; f < out_len; ++f) {
        const float n = (norm[f] > 0.0001f) ? (1.0f / norm[f]) : 1.0f;
        for (uint32_t ch = 0; ch < channels; ++ch) {
            (*out_pcm)[f * channels + ch] *= n;
        }
    }
    *out_frames = out_len;
}

static int normalize_bpm_local(int bpm) {
    if (bpm <= 0) return 120;
    if (bpm < 61) bpm *= 2;
    while (bpm < 40) bpm *= 2;
    while (bpm > 200) bpm = (int)std::lround(bpm * 0.5f);
    return std::clamp(bpm, 40, 200);
}

float audio_velocity_gain_from_midi(int velocity) {
    const float norm = std::clamp(velocity, 1, 127) / 127.0f;
    // Attenuation-only curve: full velocity preserves current sample level,
    // lower velocities only reduce it.
    return std::pow(norm, 1.5f);
}

static int derive_sample_bpm_unlocked(const Audio* a, const Sample& s) {
    const uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 120;
    const uint32_t start = std::min(s.start_frame, size - 1);
    const uint32_t end = std::clamp((s.end_frame == 0 ? size : s.end_frame), start + 1, size);
    const uint32_t span = std::max(1u, end - start);
    const float seconds = span / (float)a->cfg.sample_rate;
    if (seconds <= 0.0f) return 120;
    int bpm = (int)std::lround(60.0f / seconds);
    if (s.bpm_adjust < 0) bpm = (int)std::lround(bpm * 0.5f);
    else if (s.bpm_adjust > 0) bpm *= 2;
    return normalize_bpm_local(bpm);
}

static uint32_t sample_span_frames_unlocked(const Sample& s) {
    const uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 0;
    const uint32_t start = std::min(s.start_frame, size - 1);
    const uint32_t end = std::clamp((s.end_frame == 0 ? size : s.end_frame), start + 1, size);
    return end - start;
}

static uint32_t effective_playback_frames_unlocked(const Audio* a, const Sample& s, bool reverse) {
    if (reverse) return sample_span_frames_unlocked(s);
    const uint32_t span = sample_span_frames_unlocked(s);
    if (span == 0) return 0;
    const int sample_bpm = derive_sample_bpm_unlocked(a, s);
    const int pattern_bpm = audio_get_pattern_bpm(const_cast<Audio*>(a));
    const int time_mode = std::clamp(s.time_mode, 0, 2);
    const int target_bpm = (time_mode == 1) ? std::clamp(s.time_target_bpm, 40, 200)
                         : (time_mode == 2) ? pattern_bpm
                         : sample_bpm;
    const bool use_time_modify = time_mode != 0 &&
                                 sample_bpm >= 40 && sample_bpm <= 200 &&
                                 target_bpm >= 40 && target_bpm <= 200;
    if (!use_time_modify) return span;
    const float stretch_ratio = std::clamp(sample_bpm / (float)target_bpm, 0.5f, 2.0f);
    return std::max(1u, (uint32_t)std::lround(span * stretch_ratio));
}

static uint32_t record_interleaved_stereo_to_buffer(Audio* a, const float* in, ma_uint32 frames, float gain) {
    if (!a || !in) return 0;
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
    return frames_written;
}

// ─── Config ───────────────────────────────────────────────────────────────────

void audio_config_default(AudioConfig* cfg) {
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->sample_rate   = 44100;
    cfg->buffer_frames = 512;
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

static void playback_cb(ma_device* dev, void* out_raw, const void*, ma_uint32 frames) {
    Audio* a   = static_cast<Audio*>(dev->pUserData);
    float* out = static_cast<float*>(out_raw);
    const uint32_t n = frames * 2;

    std::memset(out, 0, n * sizeof(float));

    // Reset bus effect ring-buffer when the active effect type has changed
    if (a->fx_state_reset_pending.exchange(false, std::memory_order_acq_rel)) {
        a->global_fx_state.clear();
    }

    // Snapshot effect params and routing once (lockless atomics)
    const EffectParams fx_params{
        a->fx_p1.load(std::memory_order_relaxed),
        a->fx_p2.load(std::memory_order_relaxed),
        a->fx_p3.load(std::memory_order_relaxed),
    };
    const uint32_t sample_rate = a->cfg.sample_rate;

    // Determine if the globally active effect is a bus effect (process_buffer)
    const int  active_btn = a->fx_active_btn.load(std::memory_order_relaxed);
    const int  mfx_sub    = a->fx_mfx_type.load(std::memory_order_relaxed);
    const int  bus_def_idx = [&]() -> int {
        int idx = fx_btn_to_index(active_btn, mfx_sub);
        if (idx >= 0 && idx < FX_SLOT_COUNT && FX_DEFS[idx].process_buffer != nullptr)
            return idx;
        return -1;
    }();
    const bool has_bus_fx = (bus_def_idx >= 0);

    // Clear wet-bus scratch for this callback
    if (has_bus_fx) {
        std::fill(a->fx_wet_buf.begin(),
                  a->fx_wet_buf.begin() + std::min<size_t>(n, a->fx_wet_buf.size()),
                  0.0f);
    }

    float peak = 0.0f;
    float stereo_diff_peak = 0.0f;
    {
        std::lock_guard<std::mutex> lock(a->voice_mutex);
        for (auto& v : a->voices) {
            if (!v.active || v.slot < 0) continue;
            const auto& s = a->samples[v.slot];
            const uint32_t src_channels = v.use_rendered ? std::max(1u, v.rendered_channels) : std::max(1u, s.channels);
            const uint32_t src_frames = v.use_rendered
                ? v.rendered_frames
                : (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
            if ((!v.use_rendered && s.pcm.empty()) || src_frames == 0) { v.active = false; continue; }
            if (v.end_position == 0 || v.end_position > src_frames)
                v.end_position = src_frames;
            const bool v_pitch = v.fx_enabled && (v.fx_def_idx == fx_btn_to_index(BTN_PITCH, 0));

            // Does this voice feed the bus effect wet bus?
            const bool v_bus = has_bus_fx && v.fx_enabled && (v.fx_def_idx == bus_def_idx);

            for (uint32_t f = 0; f < frames; ++f) {
                if (!v.reverse) {
                    if (v.position >= v.end_position) {
                        if (v.looping) {
                            v.position = v.loop_start;
                        } else {
                            v.active = false;
                            break;
                        }
                    }
                } else {
                    if (v.position < v.loop_start || v.position >= v.end_position) {
                        if (v.looping) {
                            v.position = v.end_position - 1;
                        } else {
                            v.active = false;
                            break;
                        }
                    }
                }

                float l, r;
                uint32_t play_pos = v.position;
                if (v.use_rendered) {
                    if (src_channels == 2) {
                        uint32_t idx = play_pos * 2;
                        l = v.rendered_pcm[idx]     * (v.gain * v.velocity * s.level);
                        r = v.rendered_pcm[idx + 1] * (v.gain * v.velocity * s.level);
                    } else {
                        l = r = v.rendered_pcm[play_pos] * (v.gain * v.velocity * s.level);
                    }
                } else if (s.channels == 2) {
                    uint32_t idx = play_pos * 2;
                    l = s.pcm[idx]     * (v.gain * v.velocity * s.level);
                    r = s.pcm[idx + 1] * (v.gain * v.velocity * s.level);
                } else {
                    l = r = s.pcm[play_pos] * (v.gain * v.velocity * s.level);
                }

                if (!v.reverse) {
                    v.position++;
                } else if (v.position > 0) {
                    v.position--;
                } else {
                    v.position = UINT32_MAX;
                }

                if (v_pitch) {
                    const float rate = pitch_playback_rate(fx_params);
                    const float feedback = pitch_feedback_amount(fx_params);
                    const float wet = pitch_direct_effect_mix(fx_params);
                    const float dry = 1.0f - wet;

                    float wet_l = v.use_rendered
                        ? read_pcm_linear(v.rendered_pcm, src_channels, v.effect_position, 0) * (v.gain * v.velocity * s.level)
                        : read_sample_linear(s, v.effect_position, 0) * (v.gain * v.velocity * s.level);
                    float wet_r = (src_channels == 2)
                        ? (v.use_rendered
                            ? read_pcm_linear(v.rendered_pcm, src_channels, v.effect_position, 1) * (v.gain * v.velocity * s.level)
                            : read_sample_linear(s, v.effect_position, 1) * (v.gain * v.velocity * s.level))
                        : wet_l;

                    wet_l += v.fx_state.s1[0] * feedback;
                    wet_r += v.fx_state.s1[1] * feedback;
                    v.fx_state.s1[0] = wet_l;
                    v.fx_state.s1[1] = wet_r;

                    l = l * dry + wet_l * wet;
                    r = r * dry + wet_r * wet;

                    float next_pos = v.effect_position + (v.reverse ? -rate : rate);
                    if (!v.reverse) {
                        if (next_pos >= (float)v.end_position) {
                            if (v.looping) next_pos = (float)v.loop_start;
                            else next_pos = (float)v.end_position;
                        }
                    } else {
                        if (next_pos < (float)v.loop_start) {
                            if (v.looping) next_pos = (float)(v.end_position - 1);
                            else next_pos = (float)v.loop_start;
                        }
                    }
                    v.effect_position = next_pos;
                }

                // Insert effect (process_frame non-null): applied inline per-sample
                if (v.fx_enabled && v.fx_def_idx >= 0 && v.fx_def_idx < FX_SLOT_COUNT
                    && !v_pitch &&
                    FX_DEFS[v.fx_def_idx].process_frame != nullptr) {
                    float lr[2] = {l, r};
                    FX_DEFS[v.fx_def_idx].process_frame(
                        lr, s.channels, fx_params, v.fx_state, sample_rate);
                    l = lr[0];
                    r = lr[1];
                }

                if (v_bus) {
                    // Route into wet bus — process_buffer will mix echoes into out
                    a->fx_wet_buf[f * 2]     += l;
                    a->fx_wet_buf[f * 2 + 1] += r;
                } else {
                    out[f * 2]     += l;
                    out[f * 2 + 1] += r;
                }
            }
        }
    }

    // Apply bus effect (e.g. delay) to wet bus and add result to output
    if (has_bus_fx && a->global_fx_state.capacity_frames > 0) {
        FX_DEFS[bus_def_idx].process_buffer(
            a->fx_wet_buf.data(), frames,
            fx_params, a->global_fx_state, sample_rate);
        for (uint32_t i = 0; i < n; ++i)
            out[i] += a->fx_wet_buf[i];
    }

    const float gain = a->output_gain.load(std::memory_order_relaxed);
    int metro_frames_left = a->metronome_frames_left.load(std::memory_order_relaxed);
    const int metro_level = a->metronome_level.load(std::memory_order_relaxed);
    const int metro_accent = a->metronome_accent.load(std::memory_order_relaxed);
    if (metro_frames_left > 0 && metro_level > 0) {
        const float amp = (metro_level / 127.0f) * (metro_accent == 2 ? 0.24f : 0.10f);
        const float brightness = (metro_accent == 2) ? 0.92f : 0.42f;
        const int total_click_frames = std::max(1, (int)(a->cfg.sample_rate * METRONOME_MS / 1000));
        for (uint32_t f = 0; f < frames && metro_frames_left > 0; ++f, --metro_frames_left) {
            a->metronome_noise_state = a->metronome_noise_state * 1664525u + 1013904223u;
            float raw = (((a->metronome_noise_state >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;
            float click = raw - (a->metronome_prev_noise * brightness);
            a->metronome_prev_noise = raw;
            float env = metro_frames_left / (float)total_click_frames;
            float accent_env = 1.0f;
            if (metro_accent == 2) {
                float attack = metro_frames_left / (float)total_click_frames;
                accent_env = 0.75f + (attack * 0.75f);
            }
            float s = click * amp * env * accent_env;
            out[f * 2] += s;
            out[f * 2 + 1] += s;
        }
    }
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
    a->metronome_frames_left.store(metro_frames_left, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(a->rec_mutex);
        if (a->rec_active && a->rec_from_output && !a->rec_buffer.empty()) {
            float gain = a->input_gain.load(std::memory_order_relaxed);
            uint32_t frames_written = record_interleaved_stereo_to_buffer(a, out, frames, gain);
            static int dbg_out = 0;
            if (++dbg_out >= 100) {
                dbg_out = 0;
                std::printf("[RESAMPLE] frames_written=%u total=%u peak=%.3f gain=%.2f\n",
                            frames_written, a->rec_frames, peak, gain);
            }
        }
    }
}

static void capture_cb(ma_device* dev, void* out_raw, const void* in_raw, ma_uint32 frames) {
    (void)out_raw;
    Audio* a         = static_cast<Audio*>(dev->pUserData);
    const float* in  = static_cast<const float*>(in_raw);

    float peak = 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        float s = (std::abs(in[i * 2]) + std::abs(in[i * 2 + 1])) * 0.5f;
        peak = std::max(peak, s);
        sum += s;
    }
    float avg = frames > 0 ? sum / frames : 0.0f;
    a->input_peak.store(peak, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(a->rec_mutex);
    if (!a->rec_active || a->rec_buffer.empty() || a->rec_from_output) return;

    float gain = a->input_gain.load(std::memory_order_relaxed);
    uint32_t frames_written = record_interleaved_stereo_to_buffer(a, in, frames, gain);

    static int dbg_count = 0;
    if (++dbg_count >= 100) {
        dbg_count = 0;
        std::printf("[REC] frames_written=%u total=%u peak=%.3f gain=%.2f\n",
                    frames_written, a->rec_frames, peak, gain);
    }
}

// ─── Device enumeration ───────────────────────────────────────────────────────

static void cache_devices(Audio* a) {
    ma_device_info* p_out;  ma_uint32 n_out;
    ma_device_info* p_in;   ma_uint32 n_in;
    if (ma_context_get_devices(&a->context, &p_out, &n_out, &p_in, &n_in) != MA_SUCCESS)
        return;
    a->out_devs.assign(p_out, p_out + n_out);
    a->in_devs .assign(p_in,  p_in  + n_in);
}

// ─── Open / close playback device ────────────────────────────────────────────

static bool open_playback(Audio* a) {
    ma_device_config dcfg      = ma_device_config_init(ma_device_type_playback);
    dcfg.playback.format       = ma_format_f32;
    dcfg.playback.channels     = 2;
    dcfg.sampleRate            = a->cfg.sample_rate;
    dcfg.periodSizeInFrames    = a->cfg.buffer_frames;
    dcfg.dataCallback          = playback_cb;
    dcfg.pUserData             = a;

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

static bool open_capture(Audio* a) {
    ma_device_config dcfg = ma_device_config_init(ma_device_type_capture);
    dcfg.capture.format = ma_format_f32;
    dcfg.capture.channels = 2;
    dcfg.sampleRate = a->cfg.sample_rate;
    dcfg.periodSizeInFrames = a->cfg.buffer_frames;
    dcfg.dataCallback = capture_cb;
    dcfg.pUserData = a;

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

static void close_playback(Audio* a) {
    if (a->playback_ok) {
        ma_device_uninit(&a->playback);
        a->playback_ok = false;
    }
}

static void close_capture(Audio* a) {
    if (a->capture_ok) {
        ma_device_uninit(&a->capture);
        a->capture_ok = false;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Audio* audio_create(const AudioConfig* cfg) {
    Audio* a = new (std::nothrow) Audio();
    if (!a) return nullptr;

    if (cfg) a->cfg = *cfg;
    else     audio_config_default(&a->cfg);

    if (ma_context_init(nullptr, 0, nullptr, &a->context) != MA_SUCCESS) {
        delete a;
        return nullptr;
    }
    a->context_ok = true;
    cache_devices(a);

    a->playback_ok = open_playback(a);
    a->capture_ok = open_capture(a);

    a->rec_buffer.resize(AUDIO_MAX_FRAMES);

    // Pre-allocate bus effect buffers (delay ring-buffer + wet-bus scratch)
    {
        const uint32_t cap = std::max(2048u, (uint32_t)(a->cfg.sample_rate * BUS_EFFECT_MAX_SEC));
        a->global_fx_state.capacity_frames = cap;
        a->global_fx_state.buf.assign(cap * 2, 0.0f);
        a->fx_wet_buf.assign(a->cfg.buffer_frames * 2, 0.0f);
    }

    return a;
}

void audio_destroy(Audio* a) {
    if (!a) return;
    close_capture(a);
    close_playback(a);
    if (a->context_ok) ma_context_uninit(&a->context);
    delete a;
}

// ─── Samples ──────────────────────────────────────────────────────────────────

void audio_load_sample(Audio* a, int slot, const float* pcm, uint32_t frames) {
    audio_load_sample_impl(a, slot, pcm, frames);
}

void audio_clear_sample(Audio* a, int slot) {
    audio_clear_sample_impl(a, slot);
}

void audio_swap_samples(Audio* a, int slot_a, int slot_b) {
    audio_swap_samples_impl(a, slot_a, slot_b);
}

bool audio_has_sample(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return !a->samples[slot].pcm.empty();
}

bool audio_export_sample(Audio* a, int slot, std::vector<float>* pcm, uint32_t* channels, uint32_t* frames) {
    if (!a || !pcm || !channels || !frames || slot < 0 || slot >= AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    if (s.pcm.empty()) return false;
    *pcm = s.pcm;
    *channels = std::max(1u, s.channels);
    *frames = (uint32_t)(s.pcm.size() / *channels);
    return true;
}

bool audio_import_sample(Audio* a, int slot, const float* pcm, uint32_t frames, uint32_t channels) {
    if (!a || !pcm || slot < 0 || slot >= AUDIO_SLOTS) return false;
    channels = std::max(1u, channels);
    if (frames == 0) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    s.pcm.assign(pcm, pcm + (frames * channels));
    s.channels = channels;
    s.level = 1.0f;
    s.start_frame = 0;
    s.end_frame = frames;
    s.bpm_adjust = 0;
    s.time_mode = 0;
    s.time_target_bpm = -1;
    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) v.active = false;
    }
    return true;
}

void audio_set_sample_start(Audio* a, int slot, int value) {
    audio_set_sample_start_impl(a, slot, value);
}

void audio_set_sample_end(Audio* a, int slot, int value) {
    audio_set_sample_end_impl(a, slot, value);
}

int audio_get_sample_start(Audio* a, int slot) {
    return audio_get_sample_start_impl(a, slot);
}

int audio_get_sample_end(Audio* a, int slot) {
    return audio_get_sample_end_impl(a, slot);
}

bool audio_truncate_sample(Audio* a, int slot) {
    return audio_truncate_sample_impl(a, slot);
}

bool audio_quantize_sample_end_to_bpm(Audio* a, int slot, int bpm) {
    return audio_quantize_sample_end_to_bpm_impl(a, slot, bpm);
}

void audio_set_sample_level(Audio* a, int slot, int level) {
    audio_set_sample_level_impl(a, slot, level);
}

int audio_get_sample_level(Audio* a, int slot) {
    return audio_get_sample_level_impl(a, slot);
}

int audio_get_sample_bpm(Audio* a, int slot) {
    return audio_get_sample_bpm_impl(a, slot);
}

void audio_set_sample_bpm_adjust(Audio* a, int slot, int adjust) {
    audio_set_sample_bpm_adjust_impl(a, slot, adjust);
}

int audio_get_sample_bpm_adjust(Audio* a, int slot) {
    return audio_get_sample_bpm_adjust_impl(a, slot);
}

void audio_set_sample_time_mode(Audio* a, int slot, int mode, int target_bpm) {
    audio_set_sample_time_mode_impl(a, slot, mode, target_bpm);
}

int audio_get_sample_time_mode(Audio* a, int slot) {
    return audio_get_sample_time_mode_impl(a, slot);
}

int audio_get_sample_time_target_bpm(Audio* a, int slot) {
    return audio_get_sample_time_target_bpm_impl(a, slot);
}

void audio_set_pattern_bpm(Audio* a, int bpm) {
    if (!a) return;
    a->pattern_bpm.store(std::clamp(bpm, 40, 200), std::memory_order_relaxed);
}

int audio_get_pattern_bpm(Audio* a) {
    if (!a) return 120;
    return std::clamp(a->pattern_bpm.load(std::memory_order_relaxed), 40, 200);
}

void audio_set_delay_bpm(Audio* a, float bpm) {
    if (!a) return;
    a->global_fx_state.bpm = bpm > 0.0f ? bpm : 120.0f;
}

void audio_trigger_metronome(Audio* a, int level, bool accent) {
    if (!a) return;
    a->metronome_level.store(std::clamp(level, 0, 127), std::memory_order_relaxed);
    a->metronome_accent.store(accent ? 2 : 1, std::memory_order_relaxed);
    a->metronome_frames_left.store((int)(a->cfg.sample_rate * METRONOME_MS / 1000), std::memory_order_relaxed);
    a->metronome_prev_noise = 0.0f;
}

int audio_get_sample_playhead(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    const uint32_t sample_size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (sample_size <= 1) return 0;

    for (const auto& v : a->voices) {
        if (!v.active || v.slot != slot) continue;
        uint32_t abs_frame = 0;
        if (v.use_rendered && v.rendered_frames > 1) {
            float norm = std::clamp(v.position / (float)(v.rendered_frames - 1), 0.0f, 1.0f);
            uint32_t start = std::min(s.start_frame, sample_size - 1);
            uint32_t end = std::clamp((s.end_frame == 0 ? sample_size : s.end_frame), start + 1, sample_size);
            uint32_t span = (end > start) ? (end - start) : 1u;
            abs_frame = start + (uint32_t)std::lround(norm * (float)(span - 1));
        } else {
            abs_frame = std::min(v.position, sample_size - 1);
        }
        return std::clamp((int)std::lround((abs_frame / (float)(sample_size - 1)) * 127.0f), 0, 127);
    }

    return std::clamp((int)std::lround((s.start_frame / (float)(sample_size - 1)) * 127.0f), 0, 127);
}

int audio_get_pad_led_hold_frames(Audio* a, int slot, bool reverse) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    if (s.pcm.empty()) return 0;
    const uint32_t playback_frames = effective_playback_frames_unlocked(a, s, reverse);
    const float seconds = playback_frames / (float)std::max(1u, a->cfg.sample_rate);
    return std::max(1, (int)std::lround(seconds * 60.0f));
}

// ─── Playback ─────────────────────────────────────────────────────────────────

bool audio_is_playing(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    for (const auto& v : a->voices) {
        if (v.active && v.slot == slot) return true;
    }
    return false;
}

void audio_note_off(Audio* a, int slot) {
    audio_stop(a, slot);
}

void audio_trigger_mode_velocity(Audio* a, int slot, bool loop, bool gate, bool reverse, float velocity) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    if (s.pcm.empty()) return;
    velocity = std::clamp(velocity, 0.0f, 1.0f);

    const int bank = slot / 8;
    const int pad = (slot % 8) + 1;
    const char bank_name = static_cast<char>('A' + bank);
    const uint32_t sample_size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    const uint32_t start = std::min(s.start_frame, sample_size - 1);
    const uint32_t end = std::clamp((s.end_frame == 0 ? sample_size : s.end_frame), start + 1, sample_size);
    const int sample_bpm = derive_sample_bpm_unlocked(a, s);
    const int time_mode = std::clamp(s.time_mode, 0, 2);
    const int pattern_bpm = audio_get_pattern_bpm(a);
    const int target_bpm = (time_mode == 1) ? std::clamp(s.time_target_bpm, 40, 200)
                         : (time_mode == 2) ? pattern_bpm
                         : sample_bpm;
    const bool use_time_modify = !reverse &&
                                 time_mode != 0 &&
                                 sample_bpm >= 40 && sample_bpm <= 200 &&
                                 target_bpm >= 40 && target_bpm <= 200;
    const float stretch_ratio = use_time_modify
        ? std::clamp(sample_bpm / (float)target_bpm, 0.5f, 2.0f)
        : 1.0f;

    bool playing = false;
    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) {
            playing = true;
            break;
        }
    }

    if (!gate && loop && playing) {
        for (auto& v : a->voices) {
            if (v.active && v.slot == slot) v.active = false;
        }
        std::printf("[PAD] Stopped bank %c pad %d (slot %d, trigger-loop toggle)\n",
                    bank_name, pad, slot);
        return;
    }

    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) {
            if (use_time_modify) {
                render_time_stretch(s, start, end, stretch_ratio, a->cfg.sample_rate,
                                    &v.rendered_pcm, &v.rendered_frames);
                v.use_rendered = !v.rendered_pcm.empty() && v.rendered_frames > 0;
                v.rendered_channels = std::max(1u, s.channels);
            } else {
                v.use_rendered = false;
                v.rendered_pcm.clear();
                v.rendered_channels = std::max(1u, s.channels);
                v.rendered_frames = 0;
            }
            const uint32_t play_start = reverse ? (end - 1) : (v.use_rendered ? 0u : start);
            const uint32_t play_end = reverse ? end : (v.use_rendered ? v.rendered_frames : end);
            const uint32_t play_loop_start = reverse ? start : (v.use_rendered ? 0u : start);
            v.position = play_start;
            v.effect_position = (float)play_start;
            v.loop_start = play_loop_start;
            v.end_position = play_end;
            v.looping = loop;
            v.reverse = reverse;
            v.gain = 1.0f;
            v.velocity = velocity;

            // Refresh effect routing on every retrigger so toggling a pad's
            // effect assignment takes effect immediately on the next pad hit.
            {
                int   btn      = a->fx_active_btn.load(std::memory_order_relaxed);
                int   mfx_sub  = a->fx_mfx_type.load(std::memory_order_relaxed);
                uint32_t mask  = a->fx_pad_mask.load(std::memory_order_relaxed);
                bool  has_fx   = (btn >= 0) && ((mask >> slot) & 1u);
                v.fx_enabled = has_fx;
                v.fx_def_idx = has_fx ? fx_btn_to_index(btn, mfx_sub) : -1;
                v.fx_state   = {};
            }
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
    if (use_time_modify) {
        render_time_stretch(s, start, end, stretch_ratio, a->cfg.sample_rate,
                            &target->rendered_pcm, &target->rendered_frames);
        target->use_rendered = !target->rendered_pcm.empty() && target->rendered_frames > 0;
        target->rendered_channels = std::max(1u, s.channels);
    } else {
        target->use_rendered = false;
        target->rendered_pcm.clear();
        target->rendered_channels = std::max(1u, s.channels);
        target->rendered_frames = 0;
    }
    target->position     = reverse ? (end - 1) : (target->use_rendered ? 0u : start);
    target->effect_position = (float)target->position;
    target->loop_start   = reverse ? start : (target->use_rendered ? 0u : start);
    target->end_position = reverse ? end : (target->use_rendered ? target->rendered_frames : end);
    target->gain         = 1.0f;
    target->velocity     = velocity;
    target->looping      = loop;
    target->reverse      = reverse;
    target->active       = true;

    // Capture effect routing at trigger time
    {
        int   btn      = a->fx_active_btn.load(std::memory_order_relaxed);
        int   mfx_sub  = a->fx_mfx_type.load(std::memory_order_relaxed);
        uint32_t mask  = a->fx_pad_mask.load(std::memory_order_relaxed);
        bool  has_fx   = (btn >= 0) && ((mask >> slot) & 1u);
        target->fx_enabled = has_fx;
        target->fx_def_idx = has_fx ? fx_btn_to_index(btn, mfx_sub) : -1;
        target->fx_state   = {};
    }
    std::printf("[PAD] Played bank %c pad %d (slot %d)%s%s\n",
                bank_name, pad, slot,
                loop ? " [LOOP]" : "",
                gate ? " [GATE]" : "",
                reverse ? " [REV]" : "");
}

void audio_trigger_mode(Audio* a, int slot, bool loop, bool gate, bool reverse) {
    audio_trigger_mode_velocity(a, slot, loop, gate, reverse, 1.0f);
}

void audio_trigger(Audio* a, int slot) {
    audio_trigger_mode_velocity(a, slot, false, false, false, 1.0f);
}

void audio_trigger_velocity(Audio* a, int slot, float velocity) {
    audio_trigger_mode_velocity(a, slot, false, false, false, velocity);
}

void audio_stop(Audio* a, int slot) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    for (auto& v : a->voices)
        if (v.active && v.slot == slot) v.active = false;
}

// ─── Metering ─────────────────────────────────────────────────────────────────

float audio_peak(Audio* a) {
    if (!a) return 0.0f;
    return a->output_peak.load(std::memory_order_relaxed);
}

float audio_stereo_diff_peak(Audio* a) {
    if (!a) return 0.0f;
    return a->stereo_diff_peak.load(std::memory_order_relaxed);
}

void audio_set_output_gain(Audio* a, float gain) {
    if (!a) return;
    a->output_gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
}

float audio_input_peak(Audio* a) {
    if (!a) return 0.0f;
    return a->input_peak.load(std::memory_order_relaxed);
}

void audio_set_input_gain(Audio* a, float gain) {
    if (!a) return;
    a->input_gain.store(std::clamp(gain, 0.0f, 2.0f), std::memory_order_relaxed);
}

// ─── Device enumeration ───────────────────────────────────────────────────────

int audio_list_outputs(Audio* a, AudioDeviceInfo* out, int max) {
    if (!a || !out || max <= 0) return 0;
    int n = std::min((int)a->out_devs.size(), max);
    for (int i = 0; i < n; ++i) {
        std::strncpy(out[i].name, a->out_devs[i].name, AUDIO_NAME_LEN - 1);
        out[i].name[AUDIO_NAME_LEN - 1] = '\0';
        out[i].is_default = (a->out_devs[i].isDefault != 0);
    }
    return n;
}

int audio_list_inputs(Audio* a, AudioDeviceInfo* out, int max) {
    if (!a || !out || max <= 0) return 0;
    int n = std::min((int)a->in_devs.size(), max);
    for (int i = 0; i < n; ++i) {
        std::strncpy(out[i].name, a->in_devs[i].name, AUDIO_NAME_LEN - 1);
        out[i].name[AUDIO_NAME_LEN - 1] = '\0';
        out[i].is_default = (a->in_devs[i].isDefault != 0);
    }
    return n;
}

// ─── Recording ────────────────────────────────────────────────────────────────

void audio_start_recording(Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    uint32_t multiplier = 1;
    if (a->rec_quality == AUDIO_QUALITY_LONG) multiplier = 2;
    else if (a->rec_quality == AUDIO_QUALITY_LOFI) multiplier = 4;
    a->rec_max_frames = AUDIO_MAX_FRAMES * multiplier;
    a->rec_buffer.resize(a->rec_stereo ? (a->rec_max_frames * 2) : a->rec_max_frames);
    a->rec_write_pos = 0;
    a->rec_frames = 0;
    a->rec_full = false;
    a->rec_active = true;
}

void audio_stop_recording(Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_active = false;
}

bool audio_is_recording(Audio* a) {
    if (!a) return false;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    return a->rec_active;
}

bool audio_recording_full(Audio* a) {
    if (!a) return false;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    return a->rec_full;
}

bool audio_assign_recording(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return false;
    std::vector<float> recorded;
    uint32_t recorded_channels = 1;
    {
        std::lock_guard<std::mutex> lock(a->rec_mutex);

        if (a->rec_frames == 0 || a->rec_buffer.empty()) return false;

        recorded_channels = a->rec_stereo ? 2u : 1u;
        recorded.reserve(a->rec_frames * recorded_channels);

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

        a->rec_frames = 0;
        a->rec_write_pos = 0;
        a->rec_full = false;
    }

    float min_val = 0.0f, max_val = 0.0f, sum = 0.0f;
    for (float s : recorded) {
        min_val = std::min(min_val, s);
        max_val = std::max(max_val, s);
        sum += std::abs(s);
    }
    float avg = recorded.empty() ? 0.0f : sum / recorded.size();
    std::printf("[ASSIGN] slot=%d frames=%zu min=%.4f max=%.4f avg=%.4f\n",
                slot, recorded.size(), min_val, max_val, avg);

    {
        std::lock_guard<std::mutex> vlock(a->voice_mutex);
        a->samples[slot].pcm = std::move(recorded);
        a->samples[slot].channels = recorded_channels;
        a->samples[slot].level = 1.0f;
        a->samples[slot].start_frame = 0;
        a->samples[slot].end_frame = (uint32_t)(a->samples[slot].pcm.size() / a->samples[slot].channels);
        a->samples[slot].bpm_adjust = 0;
        a->samples[slot].time_mode = 0;
        a->samples[slot].time_target_bpm = -1;
    }

    return true;
}

void audio_cancel_recording(Audio* a) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_active = false;
    a->rec_frames = 0;
    a->rec_write_pos = 0;
    a->rec_full = false;
}

void audio_set_recording_mode(Audio* a, bool stereo, AudioQuality quality) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_stereo = stereo;
    a->rec_quality = quality;
}

void audio_set_record_from_output(Audio* a, bool enabled) {
    if (!a) return;
    std::lock_guard<std::mutex> lock(a->rec_mutex);
    a->rec_from_output = enabled;
}

// ─── Effect routing ───────────────────────────────────────────────────────────

void audio_set_effect_routing(Audio* a, int active_btn, int mfx_type, uint32_t pad_mask) {
    if (!a) return;
    // Detect effect/subtype change — signal the callback to flush shared bus state.
    const int prev_btn = a->fx_active_btn.load(std::memory_order_relaxed);
    const int prev_mfx = a->fx_mfx_type.load(std::memory_order_relaxed);
    const int prev_idx = fx_btn_to_index(prev_btn, prev_mfx);
    const int next_idx = fx_btn_to_index(active_btn, mfx_type);
    if (active_btn != prev_btn || mfx_type != prev_mfx || prev_idx != next_idx) {
        a->fx_state_reset_pending.store(true, std::memory_order_release);
    }
    a->fx_active_btn.store(active_btn,  std::memory_order_relaxed);
    a->fx_mfx_type.store(mfx_type,      std::memory_order_relaxed);
    a->fx_pad_mask.store(pad_mask,       std::memory_order_relaxed);
}

void audio_set_effect_params(Audio* a, float p1, float p2, float p3) {
    if (!a) return;
    a->fx_p1.store(p1, std::memory_order_relaxed);
    a->fx_p2.store(p2, std::memory_order_relaxed);
    a->fx_p3.store(p3, std::memory_order_relaxed);
}

// ─── Reconfigure ──────────────────────────────────────────────────────────────

bool audio_reconfigure(Audio* a, const AudioConfig* cfg) {
    if (!a || !cfg) return false;
    close_capture(a);
    close_playback(a);
    a->cfg = *cfg;
    a->playback_ok = open_playback(a);
    a->capture_ok = open_capture(a);
    a->rec_buffer.resize(AUDIO_MAX_FRAMES);

    // Re-allocate bus effect buffers for the new sample rate / buffer size
    {
        const uint32_t cap = std::max(2048u, (uint32_t)(a->cfg.sample_rate * BUS_EFFECT_MAX_SEC));
        a->global_fx_state.capacity_frames = cap;
        a->global_fx_state.buf.assign(cap * 2, 0.0f);
        a->global_fx_state.clear();
        a->fx_wet_buf.assign(a->cfg.buffer_frames * 2, 0.0f);
    }

    return a->playback_ok && a->capture_ok;
}

} // namespace sp303
