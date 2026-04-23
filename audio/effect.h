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
    // Biquad states — FILTER+DRIVE uses s1/s2; ISOLATOR uses s1/s2 (LP) and s3/s4 (HP)
    float s1[2] = {};
    float s2[2] = {};
    float s3[2] = {};
    float s4[2] = {};

    // VINYL SIM state
    float    s5[2]             = {};             // noise 1-pole LP per channel
    float    flutter_buf[2][256] = {};           // per-channel delay line (~5.3 ms at 48 kHz)
    uint32_t flutter_write     = 0;              // write head in flutter_buf
    float    lfo_phase         = 0.0f;           // wow/flutter LFO accumulator (radians)
    uint32_t rand_state        = 0x12345678u;    // per-voice PRNG seed
};

// ─── Global bus state (bus effects: delay, reverb, …) ────────────────────────
// One instance lives in Audio.  Zeroed when the active effect changes.

struct GlobalEffectState {
    std::vector<float> buf;             // pre-allocated, stereo interleaved
    uint32_t           write_pos      = 0;
    uint32_t           capacity_frames = 0;
    float              bpm            = 120.0f;  // used by note-synced effects (delay)

    // Reverb auxiliary state: 4 comb + 2 allpass write heads and per-comb LP state.
    uint32_t aux_pos[6]      = {};
    float    aux_state[6][2] = {};
    float    phase1          = 0.0f;
    float    phase2          = 0.0f;
    float    feedback[2]     = {};
    float    ap_x1[8][2]     = {};
    float    ap_y1[8][2]     = {};

    void clear() {
        std::fill(buf.begin(), buf.end(), 0.0f);
        write_pos = 0;
        std::fill(std::begin(aux_pos), std::end(aux_pos), 0u);
        for (auto& row : aux_state) row[0] = row[1] = 0.0f;
        phase1 = 0.0f;
        phase2 = 0.0f;
        feedback[0] = feedback[1] = 0.0f;
        for (auto& row : ap_x1) row[0] = row[1] = 0.0f;
        for (auto& row : ap_y1) row[0] = row[1] = 0.0f;
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

constexpr int FX_SLOT_COUNT = 11;
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

// Returns 3 segment bytes for the delay time display (p1 = 0..1 = note index 0..12).
// Byte 7 of each segment = decimal-point flag (matches sp303 display encoding).
void delay_note_display(float p1, uint8_t out[3]);

void isolator_frame(float* lr, uint32_t channels,
                    const EffectParams& p, EffectVoiceState& st,
                    uint32_t sample_rate);

void vinyl_sim_frame(float* lr, uint32_t channels,
                     const EffectParams& p, EffectVoiceState& st,
                     uint32_t sample_rate);

void reverb_process_buffer(float* lr_interleaved, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate);

void tape_echo_process_buffer(float* lr_interleaved, uint32_t frames,
                              const EffectParams& p, GlobalEffectState& st,
                              uint32_t sample_rate);

void chorus_process_buffer(float* lr_interleaved, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate);

void flanger_process_buffer(float* lr_interleaved, uint32_t frames,
                            const EffectParams& p, GlobalEffectState& st,
                            uint32_t sample_rate);

void phaser_process_buffer(float* lr_interleaved, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate);

void tremolo_pan_process_buffer(float* lr_interleaved, uint32_t frames,
                                const EffectParams& p, GlobalEffectState& st,
                                uint32_t sample_rate);

float pitch_playback_rate(const EffectParams& p);
float pitch_feedback_amount(const EffectParams& p);
float pitch_direct_effect_mix(const EffectParams& p);

} // namespace sp303
