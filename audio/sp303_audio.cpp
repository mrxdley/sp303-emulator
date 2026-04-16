#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "sp303_audio.h"

#include <cstring>
#include <cmath>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>
#include <new>

// ─── Internal types ───────────────────────────────────────────────────────────

struct Sample {
    std::vector<float> pcm;  // mono f32
};

struct Voice {
    int      slot     = -1;
    uint32_t position = 0;
    float    gain     = 1.0f;
    bool     active   = false;
};

struct SP303Audio {
    ma_context context;
    ma_device  playback;
    bool       context_ok  = false;
    bool       playback_ok = false;

    Sample  samples[SP303_AUDIO_SLOTS];
    Voice   voices [SP303_AUDIO_VOICES];
    std::mutex voice_mutex;

    std::atomic<float> peak{0.0f};

    SP303AudioConfig cfg;

    // Cached device lists (filled after context_init)
    std::vector<ma_device_info> out_devs;
    std::vector<ma_device_info> in_devs;
};

// ─── Config ───────────────────────────────────────────────────────────────────

void sp303_audio_config_default(SP303AudioConfig* cfg) {
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->sample_rate   = 44100;
    cfg->buffer_frames = 512;
}

// ─── Callback ─────────────────────────────────────────────────────────────────

static void playback_cb(ma_device* dev, void* out_raw, const void*, ma_uint32 frames) {
    SP303Audio* a = static_cast<SP303Audio*>(dev->pUserData);
    float* out    = static_cast<float*>(out_raw);
    const uint32_t n = frames * 2; // stereo output

    std::memset(out, 0, n * sizeof(float));

    float peak = 0.0f;
    {
        std::lock_guard<std::mutex> lock(a->voice_mutex);
        for (auto& v : a->voices) {
            if (!v.active || v.slot < 0) continue;
            const auto& s = a->samples[v.slot];
            if (s.pcm.empty()) { v.active = false; continue; }

            for (uint32_t f = 0; f < frames; ++f) {
                if (v.position >= s.pcm.size()) { v.active = false; break; }
                float samp = s.pcm[v.position++] * v.gain;
                out[f * 2]     += samp;
                out[f * 2 + 1] += samp;
            }
        }
    }

    for (uint32_t i = 0; i < n; ++i)
        peak = std::max(peak, std::abs(out[i]));

    a->peak.store(peak, std::memory_order_relaxed);
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

static void close_playback(SP303Audio* a) {
    if (a->playback_ok) {
        ma_device_uninit(&a->playback);
        a->playback_ok = false;
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
    return a;
}

void sp303_audio_destroy(SP303Audio* a) {
    if (!a) return;
    close_playback(a);
    if (a->context_ok) ma_context_uninit(&a->context);
    delete a;
}

// ─── Samples ──────────────────────────────────────────────────────────────────

void sp303_audio_load_sample(SP303Audio* a, int slot,
                             const float* pcm, uint32_t frames) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.assign(pcm, pcm + frames);
}

void sp303_audio_clear_sample(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.clear();
}

// ─── Playback ─────────────────────────────────────────────────────────────────

void sp303_audio_trigger(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);

    // Find free voice; if none, steal the one furthest through its sample
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
    if (!target) target = &a->voices[0]; // fallback

    target->slot     = slot;
    target->position = 0;
    target->gain     = 1.0f;
    target->active   = true;
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
    return a->peak.load(std::memory_order_relaxed);
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

// ─── Reconfigure ──────────────────────────────────────────────────────────────

bool sp303_audio_reconfigure(SP303Audio* a, const SP303AudioConfig* cfg) {
    if (!a || !cfg) return false;
    close_playback(a);
    a->cfg         = *cfg;
    a->playback_ok = open_playback(a);
    return a->playback_ok;
}
