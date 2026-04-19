#pragma once

#include "sp303_audio.h"
#include "miniaudio.h"

#include <atomic>
#include <mutex>
#include <vector>

struct Sample {
    std::vector<float> pcm;  // interleaved f32
    uint32_t channels = 1;
    float level = 1.0f;      // 0.0 - 1.0 (maps to SP-303 0-127)
    uint32_t start_frame = 0;
    uint32_t end_frame = 0;  // exclusive; 0 means "use full sample"
};

struct Voice {
    int      slot     = -1;
    uint32_t position = 0;
    uint32_t loop_start = 0;
    uint32_t end_position = 0;
    float    gain     = 1.0f;
    bool     looping  = false;
    bool     active   = false;
};

struct SP303Audio {
    ma_context context;
    ma_device  playback;
    ma_device  capture;
    bool       context_ok  = false;
    bool       playback_ok = false;
    bool       capture_ok  = false;

    Sample  samples[SP303_AUDIO_SLOTS];
    Voice   voices [SP303_AUDIO_VOICES];
    std::mutex voice_mutex;

    std::atomic<float> output_peak{0.0f};
    std::atomic<float> stereo_diff_peak{0.0f};
    std::atomic<float> input_peak{0.0f};
    std::atomic<float> input_gain{1.0f};
    std::atomic<float> output_gain{1.0f};

    SP303AudioConfig cfg;

    std::vector<ma_device_info> out_devs;
    std::vector<ma_device_info> in_devs;

    std::mutex rec_mutex;
    std::vector<float> rec_buffer;
    uint32_t rec_write_pos = 0;
    uint32_t rec_frames = 0;
    bool rec_active = false;
    bool rec_full = false;
    bool rec_stereo = false;
    SP303AudioQuality rec_quality = SP303_AUDIO_QUALITY_STANDARD;
    uint32_t rec_max_frames = SP303_AUDIO_MAX_FRAMES;
};
