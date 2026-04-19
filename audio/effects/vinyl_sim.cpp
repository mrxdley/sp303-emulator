#include "effects.h"

namespace sp303 {

void vinyl_sim_init(FxState* s, int sample_rate) {
    s->type = EFFECT_VINYL_SIM;
    s->sample_rate = sample_rate;
    s->params = FxParams{0.5f, 0.5f, 0.5f};
}

void vinyl_sim_process(FxState* s, float* io, int frames) {
    (void)s;
    (void)io;
    (void)frames;
    // STUB: vinyl sim
}

} // namespace sp303
