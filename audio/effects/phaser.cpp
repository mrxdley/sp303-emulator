#include "../effect.h"

#include <algorithm>
#include <cmath>

namespace sp303 {

static inline float phaser_allpass(float x, float a, float& x1, float& y1) {
    const float y = -a * x + x1 + a * y1;
    x1 = x;
    y1 = y;
    return y;
}

void phaser_process_buffer(float* lr, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate)
{
    if (!lr) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float rate_knob = std::clamp(p.p2, 0.0f, 1.0f);
    const float rate_hz = (rate_knob <= 0.01f) ? 0.0f : (0.05f + rate_knob * 1.2f);
    const float resonance = std::clamp(p.p3, 0.0f, 1.0f) * 0.88f;
    const float depth = std::clamp(p.p1, 0.0f, 1.0f);
    const float phase_inc = (2.0f * static_cast<float>(M_PI) * rate_hz) / sr;

    for (uint32_t f = 0; f < frames; ++f) {
        const float sweep = (rate_hz > 0.0f) ? (0.5f + 0.5f * std::sin(st.phase1)) : depth;
        st.phase1 += phase_inc;
        if (st.phase1 >= 2.0f * static_cast<float>(M_PI)) st.phase1 -= 2.0f * static_cast<float>(M_PI);

        const float fc = 250.0f + sweep * 1800.0f;
        const float wc = std::tan(static_cast<float>(M_PI) * fc / sr);
        const float a = (1.0f - wc) / (1.0f + wc);

        for (uint32_t ch = 0; ch < 2; ++ch) {
            const uint32_t idx = f * 2 + ch;
            const float in = lr[idx] + st.feedback[ch] * resonance;
            float y = in;
            for (int stage = 0; stage < 4; ++stage) {
                y = phaser_allpass(y, a, st.ap_x1[stage][ch], st.ap_y1[stage][ch]);
            }
            st.feedback[ch] = y;
            lr[idx] = in * 0.55f + y * 0.45f;
        }
    }
}

} // namespace sp303
