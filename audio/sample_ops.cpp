#include "sample_ops.h"

#include <algorithm>
#include <cmath>

namespace sp303 {

static uint32_t min_gap_frames(const Audio* a, uint32_t sample_size) {
    if (sample_size <= 1) return 1;
    uint32_t gap = a->cfg.sample_rate / 10;
    return std::clamp(gap, 1u, sample_size - 1);
}

static int normalize_bpm(int bpm) {
    if (bpm <= 0) return 120;
    while (bpm < 40) bpm *= 2;
    while (bpm > 200) bpm = (int)std::lround(bpm * 0.5f);
    return std::clamp(bpm, 40, 200);
}

static int derive_base_bpm(const Audio* a, const Sample& s) {
    const uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 120;
    const uint32_t start = std::min(s.start_frame, size - 1);
    const uint32_t end = std::clamp((s.end_frame == 0 ? size : s.end_frame), start + 1, size);
    const uint32_t span = std::max(1u, end - start);
    const float seconds = span / (float)a->cfg.sample_rate;
    if (seconds <= 0.0f) return 120;
    return normalize_bpm((int)std::lround(60.0f / seconds));
}

static int apply_bpm_adjust(int bpm, int adjust) {
    if (adjust < 0) bpm = (int)std::lround(bpm * 0.5f);
    else if (adjust > 0) bpm *= 2;
    return normalize_bpm(bpm);
}

void audio_load_sample_impl(Audio* a, int slot, const float* pcm, uint32_t frames) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.assign(pcm, pcm + frames);
    a->samples[slot].channels = 1;
    a->samples[slot].start_frame = 0;
    a->samples[slot].end_frame = frames;
    a->samples[slot].level = 1.0f;
    a->samples[slot].bpm_adjust = 0;
    a->samples[slot].time_mode = 0;
    a->samples[slot].time_target_bpm = -1;
}

void audio_clear_sample_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].pcm.clear();
    a->samples[slot].channels = 1;
    a->samples[slot].level = 1.0f;
    a->samples[slot].start_frame = 0;
    a->samples[slot].end_frame = 0;
    a->samples[slot].bpm_adjust = 0;
    a->samples[slot].time_mode = 0;
    a->samples[slot].time_target_bpm = -1;
    for (auto& v : a->voices) {
        if (v.active && v.slot == slot) v.active = false;
    }
}

void audio_swap_samples_impl(Audio* a, int slot_a, int slot_b) {
    if (!a) return;
    if (slot_a < 0 || slot_a >= AUDIO_SLOTS) return;
    if (slot_b < 0 || slot_b >= AUDIO_SLOTS) return;
    if (slot_a == slot_b) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    std::swap(a->samples[slot_a], a->samples[slot_b]);
    for (auto& v : a->voices) {
        if (!v.active) continue;
        if (v.slot == slot_a) v.active = false;
        else if (v.slot == slot_b) v.active = false;
    }
}

void audio_set_sample_start_impl(Audio* a, int slot, int value) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
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

void audio_set_sample_end_impl(Audio* a, int slot, int value) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
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

int audio_get_sample_start_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 0;
    return std::clamp((int)std::lround((s.start_frame / (float)(size - 1)) * 127.0f), 0, 127);
}

int audio_get_sample_end_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 127;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    uint32_t size = (uint32_t)(s.pcm.size() / std::max(1u, s.channels));
    if (size <= 1) return 127;
    uint32_t end = (s.end_frame == 0 || s.end_frame > size) ? size : s.end_frame;
    return std::clamp((int)std::lround((end / (float)size) * 127.0f), 0, 127);
}

bool audio_truncate_sample_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return false;
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

bool audio_quantize_sample_end_to_bpm_impl(Audio* a, int slot, int bpm) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return false;
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

void audio_set_sample_level_impl(Audio* a, int slot, int level) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].level = std::clamp(level / 127.0f, 0.0f, 1.0f);
}

int audio_get_sample_level_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 127;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return std::clamp((int)std::lround(a->samples[slot].level * 127.0f), 0, 127);
}

int audio_get_sample_bpm_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 120;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    const auto& s = a->samples[slot];
    if (s.pcm.empty()) return 120;
    return apply_bpm_adjust(derive_base_bpm(a, s), s.bpm_adjust);
}

void audio_set_sample_bpm_adjust_impl(Audio* a, int slot, int adjust) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    a->samples[slot].bpm_adjust = std::clamp(adjust, -1, 1);
}

int audio_get_sample_bpm_adjust_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return std::clamp(a->samples[slot].bpm_adjust, -1, 1);
}

void audio_set_sample_time_mode_impl(Audio* a, int slot, int mode, int target_bpm) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    auto& s = a->samples[slot];
    s.time_mode = std::clamp(mode, 0, 2);
    s.time_target_bpm = (s.time_mode == 1) ? std::clamp(target_bpm, 40, 200) : -1;
}

int audio_get_sample_time_mode_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return 0;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return std::clamp(a->samples[slot].time_mode, 0, 2);
}

int audio_get_sample_time_target_bpm_impl(Audio* a, int slot) {
    if (!a || slot < 0 || slot >= AUDIO_SLOTS) return -1;
    std::lock_guard<std::mutex> lock(a->voice_mutex);
    return a->samples[slot].time_target_bpm;
}

} // namespace sp303
