#pragma once

#include "sp303_audio.h"
#include "effect.h"
#include "miniaudio.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace sp303 {

struct Sample {
    std::vector<float> pcm;  // interleaved f32
    uint32_t channels = 1;
    float level = 1.0f;      // 0.0 - 1.0 (maps to SP-303 0-127)
    uint32_t start_frame = 0;
    uint32_t end_frame = 0;  // exclusive; 0 means "use full sample"
    int bpm_adjust = 0;      // -1=half, 0=base, +1=double
    int time_mode = 0;       // 0=off, 1=custom bpm, 2=pattern bpm
    int time_target_bpm = -1;
};

struct Voice {
    int      slot         = -1;
    uint32_t position     = 0;
    float    effect_position = 0.0f;
    uint32_t loop_start   = 0;
    uint32_t end_position = 0;
    float    gain         = 1.0f;
    float    velocity     = 1.0f;
    bool     looping      = false;
    bool     reverse      = false;
    bool     active       = false;

    // Effect routing — captured at trigger time, applied per-frame in callback
    bool             fx_enabled  = false;
    int              fx_def_idx  = -1;   // index into FX_DEFS, -1 = none
    EffectVoiceState fx_state;

    bool              use_rendered = false;
    std::vector<float> rendered_pcm;
    uint32_t          rendered_channels = 1;
    uint32_t          rendered_frames = 0;
};

struct Audio {
    ma_context context;
    ma_device  playback;
    ma_device  capture;
    bool       context_ok  = false;
    bool       playback_ok = false;
    bool       capture_ok  = false;

    Sample  samples[AUDIO_SLOTS];
    Voice   voices [AUDIO_VOICES];
    std::mutex voice_mutex;

    std::atomic<float> output_peak{0.0f};
    std::atomic<float> stereo_diff_peak{0.0f};
    std::atomic<float> input_peak{0.0f};
    std::atomic<float> input_gain{1.0f};
    std::atomic<float> output_gain{1.0f};

    AudioConfig cfg;

    std::vector<ma_device_info> out_devs;
    std::vector<ma_device_info> in_devs;

    std::mutex rec_mutex;
    std::vector<float> rec_buffer;
    uint32_t rec_write_pos = 0;
    uint32_t rec_frames = 0;
    bool rec_active = false;
    bool rec_full = false;
    bool rec_stereo = false;
    bool rec_from_output = false;
    AudioQuality rec_quality = AUDIO_QUALITY_STANDARD;
    uint32_t rec_max_frames = AUDIO_MAX_FRAMES;

    // ─── Effect routing (written from controller, read from audio callback) ───
    // Lockless: all fields are atomic.  pad_mask bit i = pad i has effect.
    std::atomic<int>      fx_active_btn{-1};   // ButtonID or -1
    std::atomic<int>      fx_mfx_type{0};      // 0-20 (MFX sub-type)
    std::atomic<uint32_t> fx_pad_mask{0};       // bit i = pad i carries effect
    std::atomic<float>    fx_p1{0.5f};          // CUTOFF  knob value
    std::atomic<float>    fx_p2{0.0f};          // RESONANCE knob value
    std::atomic<float>    fx_p3{0.5f};          // DRIVE    knob value
    std::atomic<int>      pattern_bpm{120};
    std::atomic<int>      metronome_level{0};      // 0-127
    std::atomic<int>      metronome_accent{0};     // 0 none, 1 weak, 2 strong
    std::atomic<int>      metronome_frames_left{0};
    uint32_t              metronome_noise_state = 0x12345678u;
    float                 metronome_prev_noise = 0.0f;

    // ─── Bus effect state (playback callback only after init) ─────────────────
    // global_fx_state and fx_wet_buf are only ever touched by playback_cb after
    // audio_create initialises them, so no locking is needed.
    // fx_state_reset_pending bridges the controller→callback boundary locklessly.
    GlobalEffectState     global_fx_state;
    std::vector<float>    fx_wet_buf;           // scratch: frames * 2, pre-allocated
    std::atomic<bool>     fx_state_reset_pending{false};
};

} // namespace sp303
