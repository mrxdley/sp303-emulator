#include "sample_ops.h"

#include <algorithm>
#include <cmath>

static uint32_t min_gap_frames(const SP303Audio* a, uint32_t sample_size) {
    if (sample_size <= 1) return 1;
    uint32_t gap = a->cfg.sample_rate / 10; // 100ms minimum gap
    return std::clamp(gap, 1u, sample_size - 1);
}

void sp303_audio_load_sample_impl(SP303Audio* a, int slot, const float* pcm, uint32_t frames) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.assign(pcm, pcm + frames);
    a->samples[slot].channels = 1;
    a->samples[slot].start_frame = 0;
    a->samples[slot].end_frame = frames;
    a->samples[slot].level = 1.0f;
}

void sp303_audio_clear_sample_impl(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.clear();
    a->samples[slot].channels = 1;
    a->samples[slot].level = 1.0f;
    a->samples[slot].start_frame = 0;
    a->samples[slot].end_frame = 0;
}

void sp303_audio_swap_samples_impl(SP303Audio* a, int slot_a, int slot_b) {
    if (!a) return;
    if (slot_a < 0 || slot_a >= SP303_AUDIO_SLOTS) return;
    if (slot_b < 0 || slot_b >= SP303_AUDIO_SLOTS) return;
    if (slot_a == slot_b) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    std::swap(a->samples[slot_a], a->samples[slot_b]);
    for (auto& v : a->voices) {
        if (!v.active) continue;
        if (v.slot == slot_a) v.active = false;
        else if (v.slot == slot_b) v.active = false;
    }
}

void sp303_audio_set_sample_start_impl(SP303Audio* a, int slot, int value) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return;
    uint32_t end = (s.end_frame == 0 || s.end_frame > size) ? size : s.end_frame;
    uint32_t gap = min_gap_frames(a, size);
    uint32_t max_start = (end > gap) ? (end - gap) : 0;
    uint32_t target = (uint32_t)std::lround((std::clamp(value, 0, 127) / 127.0f) * (size - 1));
    s.start_frame = std::min(target, max_start);
}

void sp303_audio_set_sample_end_impl(SP303Audio* a, int slot, int value) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return;
    uint32_t gap = min_gap_frames(a, size);
    uint32_t min_end = std::min(size, s.start_frame + gap);
    uint32_t target = (uint32_t)std::lround((std::clamp(value, 0, 127) / 127.0f) * size);
    target = std::clamp(target, 1u, size);
    s.end_frame = std::max(target, min_end);
}

int sp303_audio_get_sample_start_impl(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 0;
    return std::clamp((int)std::lround((s.start_frame / (float)(size - 1)) * 127.0f), 0, 127);
}

int sp303_audio_get_sample_end_impl(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return 127;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 127;
    uint32_t end = (s.end_frame == 0 || s.end_frame > size) ? size : s.end_frame;
    return std::clamp((int)std::lround((end / (float)size) * 127.0f), 0, 127);
}

bool sp303_audio_truncate_sample_impl(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return false;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size == 0) return false;

    uint32_t start = std::min(s.start_frame, size - 1);
    uint32_t end = std::clamp((s.end_frame == 0 ? size : s.end_frame), start + 1, size);
    if (start == 0 && end == size) return true;

    std::vector<float> truncated;
    truncated.reserve((end - start) * s.channels);
    if (s.channels == 2) {
        for (uint32_t i = start; i < end; ++i) {
            truncated.push_back(s.pcm[i * 2]);
            truncated.push_back(s.pcm[i * 2 + 1]);
        }
    } else {
        truncated.assign(s.pcm.begin() + start, s.pcm.begin() + end);
    }
    s.pcm = std::move(truncated);
    s.start_frame = 0;
    s.end_frame = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));

    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) v.active = false;
    }
    return true;
}

bool sp303_audio_quantize_sample_end_to_bpm_impl(SP303Audio* a, int slot, int bpm) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return false;
    bpm = std::clamp(bpm, 40, 200);
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return false;

    uint32_t start = std::min(s.start_frame, size - 1);
    uint32_t gap = min_gap_frames(a, size);
    uint32_t usable = size - start;
    if (usable <= gap) {
        s.end_frame = size;
        return true;
    }

    float beat_frames = (a->cfg.sample_rate * 60.0f) / (float)bpm;
    if (beat_frames <= 1.0f) return false;

    uint32_t beats = std::max(1u, (uint32_t)std::lround(usable / beat_frames));
    uint32_t quantized_len = (uint32_t)std::lround(beats * beat_frames);
    quantized_len = std::clamp(quantized_len, gap, usable);
    s.end_frame = start + quantized_len;
    return true;
}

void sp303_audio_set_sample_level_impl(SP303Audio* a, int slot, int level) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].level = std::clamp(level / 127.0f, 0.0f, 1.0f);
}

int sp303_audio_get_sample_level_impl(SP303Audio* a, int slot) {
    if (!a || slot < 0 || slot >= SP303_AUDIO_SLOTS) return 127;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return std::clamp((int)std::lround(a->samples[slot].level * 127.0f), 0, 127);
}
