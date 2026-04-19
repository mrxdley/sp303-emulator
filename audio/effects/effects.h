#pragma once
#include <cstdint>
#include "../../core/sp303.h"  // For EffectType enum

namespace sp303 {

// Effect parameters - three knobs, 0.0 to 1.0
struct FxParams {
    float ctrl1 = 0.5f;
    float ctrl2 = 0.5f;
    float ctrl3 = 0.5f;
};

// Per-effect state
struct FxState {
    EffectType type = EFFECT_NONE;
    int sample_rate = 44100;
    FxParams params;

    // SVF filter state (for FILTER+DRIVE)
    float svf_v0 = 0.0f;      // input
    float svf_v1 = 0.0f;      // bandpass (ic1eq)
    float svv_v2 = 0.0f;      // lowpass (ic2eq)
    float svf_g = 0.0f;       // smoothed g coefficient
    float svf_k = 0.0f;       // 1/Q

    // Cached target values for smoothing
    float target_cutoff = -1.0f;
    float target_q = -1.0f;
};

// Effect interface
struct Effect {
    EffectType type;
    void (*init)(FxState* state, int sample_rate);
    void (*process)(FxState* state, float* io, int frames);
};

// Top-level API
void fx_init(FxState* state, EffectType type, int sample_rate);
void fx_set_params(FxState* state, const FxParams* params);
void fx_process(FxState* state, float* io, int frames);

} // namespace sp303
