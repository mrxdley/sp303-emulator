#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── ISOLATOR ─────────────────────────────────────────────────────────────────
//
// 3-band frequency isolator. Signal is split into low/mid/high bands;
// each band is independently gain-scaled then recombined.
//
// p1 (Lo ): low band gain   0 = cut, 1 = full
// p2 (Mid): mid band gain   0 = cut, 1 = full
// p3 (Hi ): high band gain  0 = cut, 1 = full
//
// Band split:
//   LOW  = LP biquad at 300 Hz    (DF2T state in s1/s2)
//   HIGH = HP biquad at 3000 Hz   (DF2T state in s3/s4)
//   MID  = input - LOW - HIGH

void isolator_frame(float* lr, uint32_t channels,
                    const EffectParams& p, EffectVoiceState& st,
                    uint32_t sample_rate)
{
    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float Q  = static_cast<float>(M_SQRT1_2);  // Butterworth: Q = 1/sqrt(2)

    // LP at 300 Hz
    const float w_lo      = 2.0f * static_cast<float>(M_PI) * 300.0f / sr;
    const float sin_lo    = std::sin(w_lo);
    const float cos_lo    = std::cos(w_lo);
    const float alpha_lo  = sin_lo / (2.0f * Q);
    const float a0i_lo    = 1.0f / (1.0f + alpha_lo);
    const float b0_lo     = (1.0f - cos_lo) * 0.5f * a0i_lo;
    const float b1_lo     = (1.0f - cos_lo) * a0i_lo;
    const float b2_lo     = b0_lo;
    const float a1_lo     = -2.0f * cos_lo * a0i_lo;
    const float a2_lo     = (1.0f - alpha_lo) * a0i_lo;

    // HP at 3000 Hz
    const float w_hi      = 2.0f * static_cast<float>(M_PI) * 3000.0f / sr;
    const float sin_hi    = std::sin(w_hi);
    const float cos_hi    = std::cos(w_hi);
    const float alpha_hi  = sin_hi / (2.0f * Q);
    const float a0i_hi    = 1.0f / (1.0f + alpha_hi);
    const float b0_hi     = (1.0f + cos_hi) * 0.5f * a0i_hi;
    const float b1_hi     = -(1.0f + cos_hi) * a0i_hi;
    const float b2_hi     = b0_hi;
    const float a1_hi     = -2.0f * cos_hi * a0i_hi;
    const float a2_hi     = (1.0f - alpha_hi) * a0i_hi;

    const uint32_t ch_count = std::min(channels, 2u);

    for (uint32_t ch = 0; ch < ch_count; ++ch) {
        const float x = lr[ch];

        // LP biquad (DF2T) — low band
        const float low  = b0_lo * x + st.s1[ch];
        st.s1[ch]        = b1_lo * x - a1_lo * low + st.s2[ch];
        st.s2[ch]        = b2_lo * x - a2_lo * low;

        // HP biquad (DF2T) — high band
        const float high = b0_hi * x + st.s3[ch];
        st.s3[ch]        = b1_hi * x - a1_hi * high + st.s4[ch];
        st.s4[ch]        = b2_hi * x - a2_hi * high;

        const float mid = x - low - high;

        lr[ch] = p.p1 * low + p.p2 * mid + p.p3 * high;
    }

    if (ch_count == 1)
        lr[1] = lr[0];
}

} // namespace sp303
