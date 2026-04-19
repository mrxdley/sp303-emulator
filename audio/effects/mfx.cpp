#include "effects.h"

namespace sp303 {

void reverb_init(FxState* s, int sample_rate) {
    s->type = EFFECT_MFX_1;
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void reverb_process(FxState* s, float* io, int frames) {
    (void)s;
    (void)io;
    (void)frames;
    // STUB: reverb
}

void tape_echo_init(FxState* s, int sample_rate) {
    s->type = EFFECT_MFX_2;
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void tape_echo_process(FxState* s, float* io, int frames) {
    (void)s;
    (void)io;
    (void)frames;
    // STUB: tape echo
}

void chorus_init(FxState* s, int sample_rate) {
    s->type = EFFECT_MFX_3;
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void chorus_process(FxState* s, float* io, int frames) {
    (void)s;
    (void)io;
    (void)frames;
    // STUB: chorus
}

void stub_init(FxState* s, int sample_rate) {
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void stub_process(FxState* s, float* io, int frames) {
    (void)s;
    (void)io;
    (void)frames;
}

} // namespace sp303
