#include "effects.h"

namespace sp303 {

void isolator_init(FxState* s, int sample_rate) {
    s->type = EFFECT_ISOLATOR;
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void isolator_process(FxState* s, float* io, int frames) {
    float low_gain = 0.5f + s->params.ctrl1 * 1.5f;
    float mid_gain = 0.5f + s->params.ctrl2 * 1.5f;
    float high_gain = 0.5f + s->params.ctrl3 * 1.5f;

    float avg = (low_gain + mid_gain + high_gain) / 3.0f;

    for (int i = 0; i < frames; i++) {
        io[i] = std::clamp(io[i] * avg, -1.0f, 1.0f);
    }
}

} // namespace sp303
