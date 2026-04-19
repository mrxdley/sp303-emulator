#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── FILTER + DRIVE ───────────────────────────────────────────────────────────
//
// Chain:  input → soft-clip drive → resonant low-pass filter → output
//
// p1 (CoF): cutoff frequency, 20 Hz – 20 kHz (log scale)
// p2 (rES): resonance (filter Q), 0.5 – 10
// p3 (drV): pre-filter overdrive gain, 1× – 30×

void filter_drive_frame(float* lr,
                        uint32_t channels,
                        const EffectParams& p,
                        EffectVoiceState& st,
                        uint32_t sample_rate)
{
    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);

    const float drive = 1.0f + p.p3 * 29.0f;
    const float fc    = std::clamp(20.0f * std::pow(1000.0f, p.p1), 20.0f, sr * 0.49f);
    const float Q     = 0.5f + p.p2 * 9.5f;

    // Biquad LP coefficients (Audio EQ Cookbook, normalised by a0)
    const float omega  = 2.0f * static_cast<float>(M_PI) * fc / sr;
    const float sin_w  = std::sin(omega);
    const float cos_w  = std::cos(omega);
    const float alpha  = sin_w / (2.0f * Q);
    const float a0_inv = 1.0f / (1.0f + alpha);
    const float b0     = (1.0f - cos_w) * 0.5f * a0_inv;
    const float b1     = (1.0f - cos_w) * a0_inv;
    const float b2     = b0;
    const float na1    = -2.0f * cos_w * a0_inv;
    const float na2    = (1.0f - alpha) * a0_inv;

    const uint32_t ch_count = std::min(channels, 2u);

    for (uint32_t ch = 0; ch < ch_count; ++ch) {
        float x    = std::tanh(lr[ch] * drive);
        float y    = b0 * x + st.s1[ch];
        st.s1[ch]  = b1 * x - na1 * y + st.s2[ch];
        st.s2[ch]  = b2 * x - na2 * y;
        lr[ch]     = y;
    }

    if (ch_count == 1)
        lr[1] = lr[0];
}

} // namespace sp303
