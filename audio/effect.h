#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

namespace sp303 {

// ─── Effect parameters ────────────────────────────────────────────────────────

struct EffectParams {
    float p1;  // 0-1, CUTOFF knob
    float p2;  // 0-1, RESONANCE knob
    float p3;  // 0-1, DRIVE knob
};

// ─── Per-voice state (insert effects only) ────────────────────────────────────
// Allocated per-voice, zeroed on voice allocation, persistent across callbacks.
// Direct-Form-II-Transposed biquad state, one pair per channel (L/R).

struct EffectVoiceState {
    float s1[2] = {};
    float s2[2] = {};
};

// ─── Global bus state (bus effects: delay, reverb, …) ────────────────────────
// One instance lives in Audio.  Zeroed when the active effect changes.

struct GlobalEffectState {
    std::vector<float> buf;             // pre-allocated, stereo interleaved
    uint32_t           write_pos      = 0;
    uint32_t           capacity_frames = 0;

    void clear() {
        std::fill(buf.begin(), buf.end(), 0.0f);
        write_pos = 0;
    }
};

// ─── Effect definition ────────────────────────────────────────────────────────
// Exactly one of process_frame / process_buffer is non-null:
//   process_frame  → insert effect, called per-voice per-sample
//   process_buffer → bus effect, called once on the mixed wet bus after all voices

struct EffectDef {
    const char* name;
    const char* p1_label;
    const char* p2_label;
    const char* p3_label;

    void (*process_frame)(float* lr, uint32_t channels,
                          const EffectParams& p, EffectVoiceState& st,
                          uint32_t sample_rate);

    void (*process_buffer)(float* lr_interleaved, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate);
};

// ─── Registry ─────────────────────────────────────────────────────────────────

constexpr int FX_SLOT_COUNT = 3;
extern const EffectDef FX_DEFS[FX_SLOT_COUNT];

// Map (active_effect_btn ButtonID, mfx_sub 0-20) → FX_DEFS index, or -1.
int fx_btn_to_index(int btn_id, int mfx_sub);

// ─── Effect entry points (one per file in audio/effects/) ─────────────────────

void filter_drive_frame  (float* lr, uint32_t channels,
                          const EffectParams& p, EffectVoiceState& st,
                          uint32_t sample_rate);

void delay_process_buffer(float* lr_interleaved, uint32_t frames,
                          const EffectParams& p, GlobalEffectState& st,
                          uint32_t sample_rate);

float pitch_playback_rate(const EffectParams& p);
float pitch_feedback_amount(const EffectParams& p);
float pitch_direct_effect_mix(const EffectParams& p);

} // namespace sp303
