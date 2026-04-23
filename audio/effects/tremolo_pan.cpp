#include "../effect.h"

#include <algorithm>
#include <cmath>

namespace sp303 {

static inline float shaped_lfo(float phase, float shape) {
    const float s = std::sin(phase);
    const float sharp = 1.0f + shape * 7.0f;
    return std::tanh(s * sharp) / std::tanh(sharp);
}

void tremolo_pan_process_buffer(float* lr, uint32_t frames,
                                const EffectParams& p, GlobalEffectState& st,
                                uint32_t sample_rate)
{
    if (!lr) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float depth = std::clamp(p.p1, 0.0f, 1.0f);
    const float rate_hz = 0.10f + std::clamp(p.p2, 0.0f, 1.0f) * 8.0f;
    const float mode = std::clamp(p.p3, 0.0f, 1.0f);
    const bool pan_mode = mode >= 0.5f;
    const float shape = pan_mode ? ((mode - 0.5f) * 2.0f) : (mode * 2.0f);
    const float phase_inc = (2.0f * static_cast<float>(M_PI) * rate_hz) / sr;

    for (uint32_t f = 0; f < frames; ++f) {
        const float wave = shaped_lfo(st.phase1, shape);
        st.phase1 += phase_inc;
        if (st.phase1 >= 2.0f * static_cast<float>(M_PI)) st.phase1 -= 2.0f * static_cast<float>(M_PI);

        float l = lr[f * 2];
        float r = lr[f * 2 + 1];
        if (!pan_mode) {
            const float gain = (1.0f - depth) + depth * (0.5f + 0.5f * wave);
            l *= gain;
            r *= gain;
        } else {
            const float pan = wave * depth;
            const float angle = (pan + 1.0f) * (static_cast<float>(M_PI) * 0.25f);
            const float gl = std::cos(angle);
            const float gr = std::sin(angle);
            l *= gl;
            r *= gr;
        }
        lr[f * 2] = l;
        lr[f * 2 + 1] = r;
    }
}

} // namespace sp303
