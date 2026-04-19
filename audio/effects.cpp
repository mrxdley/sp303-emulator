#include "effects/effects.h"
#include "effects/registry.h"
#include <cstring>

namespace sp303 {

// Effect registry - must match order of EffectType enum
static const struct {
    EffectType type;
    void (*init)(FxState*, int);
    void (*process)(FxState*, float*, int);
} REGISTRY[] = {
    { EFFECT_NONE,       stub_init,       stub_process },
    { EFFECT_FILTER_DRIVE, filter_drive_init, filter_drive_process },
    { EFFECT_PITCH,      pitch_init,      pitch_process },
    { EFFECT_DELAY,      delay_init,      delay_process },
    { EFFECT_VINYL_SIM,  vinyl_sim_init,  vinyl_sim_process },
    { EFFECT_ISOLATOR,   isolator_init,   isolator_process },
    { EFFECT_MFX_1,      reverb_init,     reverb_process },
    { EFFECT_MFX_2,      tape_echo_init,  tape_echo_process },
    { EFFECT_MFX_3,      chorus_init,     chorus_process },
    { EFFECT_MFX_4,      stub_init,       stub_process },
    { EFFECT_MFX_5,      stub_init,       stub_process },
    { EFFECT_MFX_6,      stub_init,       stub_process },
    { EFFECT_MFX_7,      stub_init,       stub_process },
    { EFFECT_MFX_8,      stub_init,       stub_process },
    { EFFECT_MFX_9,      stub_init,       stub_process },
    { EFFECT_MFX_10,     stub_init,       stub_process },
    { EFFECT_MFX_11,     stub_init,       stub_process },
    { EFFECT_MFX_12,     stub_init,       stub_process },
    { EFFECT_MFX_13,     stub_init,       stub_process },
    { EFFECT_MFX_14,     stub_init,       stub_process },
    { EFFECT_MFX_15,     stub_init,       stub_process },
    { EFFECT_MFX_16,     stub_init,       stub_process },
    { EFFECT_MFX_17,     stub_init,       stub_process },
    { EFFECT_MFX_18,     stub_init,       stub_process },
    { EFFECT_MFX_19,     stub_init,       stub_process },
    { EFFECT_MFX_20,     stub_init,       stub_process },
    { EFFECT_MFX_21,     stub_init,       stub_process },
};

void fx_init(FxState* state, EffectType type, int sample_rate) {
    if (!state) return;
    if (type < EFFECT_NONE || type >= EFFECT_COUNT) type = EFFECT_NONE;

    std::memset(state, 0, sizeof(FxState));
    state->type = type;
    state->sample_rate = sample_rate;
    state->target_cutoff = -1.0f;
    state->target_q = -1.0f;

    REGISTRY[type].init(state, sample_rate);
}

void fx_set_params(FxState* state, const FxParams* params) {
    if (!state || !params) return;
    state->params = *params;
}

void fx_process(FxState* state, float* io, int frames) {
    if (!state || !io || frames <= 0) return;
    if (state->type == EFFECT_NONE) return;

    REGISTRY[state->type].process(state, io, frames);
}

} // namespace sp303
