#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── VINYL SIM ────────────────────────────────────────────────────────────────
//
// Three-stage analog record simulation applied per-voice as an insert effect.
// Signal chain: CMP → noS → FLu
//
// p1 (CMP): tanh soft saturation — models vinyl groove limiting / "warmth"
//           0 = bypass, 1 = heavy saturation (drive 1× – 5×)
//
// p2 (noS): surface noise: 1-pole LP-colored white noise + rare crackle impulses
//           noise_gain = p2 * 0.04,  crackle_prob = p2² * 0.0002 per sample
//
// p3 (FLu): wow & flutter via delay-line pitch wobble
//           wow 0.5 Hz ±10 samples, flutter 6 Hz ±3 samples, scaled by p3
//           FLU_CENTER = 20 samples of latency at center position (~0.4 ms)

static constexpr int FLU_BUF    = 256;
static constexpr int FLU_CENTER = 100;  // max wow+flutter depth at p3=1 is ~162 samples

static inline uint32_t lcg_next(uint32_t s) {
    return s * 1664525u + 1013904223u;
}
static inline float lcg_float(uint32_t s) {
    return static_cast<float>(static_cast<int32_t>(s)) * (1.0f / 2147483648.0f);
}

void vinyl_sim_frame(float* lr, uint32_t channels,
                     const EffectParams& p, EffectVoiceState& st,
                     uint32_t sample_rate)
{
    const float sr       = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const uint32_t ch_count = std::min(channels, 2u);

    // ── p1: CMP — tanh soft saturation ──────────────────────────────────────
    // No /drive normalization — allows harmonic warmth and level character.
    // Drive 1× (p1=0) to 6× (p1=1). At drive=6 peaks clip and signal brightens.
    if (p.p1 > 0.001f) {
        const float drive = 1.0f + p.p1 * 5.0f;
        for (uint32_t ch = 0; ch < ch_count; ++ch)
            lr[ch] = std::tanh(lr[ch] * drive);
    }

    // ── p2: noS — 1-pole LP colored noise + crackle ─────────────────────────
    if (p.p2 > 0.001f) {
        const float noise_gain   = p.p2 * 0.04f;
        const float crackle_prob = p.p2 * p.p2 * 0.0002f;
        const float lp_a = std::exp(-2.0f * static_cast<float>(M_PI) * 6000.0f / sr);

        st.rand_state = lcg_next(st.rand_state);
        const float noise_raw = lcg_float(st.rand_state);

        st.rand_state = lcg_next(st.rand_state);
        float crackle = 0.0f;
        if (std::abs(lcg_float(st.rand_state)) < crackle_prob) {
            st.rand_state = lcg_next(st.rand_state);
            crackle = lcg_float(st.rand_state) * 0.4f;
        }

        for (uint32_t ch = 0; ch < ch_count; ++ch) {
            st.s5[ch] = lp_a * st.s5[ch] + (1.0f - lp_a) * noise_raw;
            lr[ch] += noise_gain * st.s5[ch] + crackle;
        }
    }

    // ── p3: FLu — wow & flutter via modulated delay line ────────────────────
    // Write current sample into circular buffer; read back at LFO-offset position.
    // Varying the read offset changes apparent playback speed → real pitch wobble.
    if (p.p3 > 0.001f) {
        const float two_pi = 2.0f * static_cast<float>(M_PI);

        // Above p3=0.5: LFO slows linearly to 0.25x at p3=1.
        const float lfo_rate = p.p3 > 0.5f
            ? 1.0f - (p.p3 - 0.5f) * 1.5f   // 1.0 @ 0.5 → 0.25 @ 1.0
            : 1.0f;
        st.lfo_phase += two_pi / sr * lfo_rate;
        if (st.lfo_phase >= two_pi)
            st.lfo_phase -= two_pi;

        const float wow_depth     = p.p3 * 90.0f;   // ~10 cents at p3=1, 0.5 Hz
        const float flutter_depth = p.p3 * 72.0f;   // ~100 cents at p3=1, 6 Hz

        const float lfo_slow = std::sin(st.lfo_phase * 0.5f);
        const float lfo_mod  = wow_depth     * lfo_slow
                             + flutter_depth * std::sin(st.lfo_phase * 6.0f);

        float read_off = static_cast<float>(FLU_CENTER) + lfo_mod;
        read_off = std::clamp(read_off, 1.0f, static_cast<float>(FLU_BUF - 2));

        const int   ri    = static_cast<int>(read_off);
        const float rfrac = read_off - static_cast<float>(ri);
        const int   fw    = static_cast<int>(st.flutter_write);

        for (uint32_t ch = 0; ch < ch_count; ++ch) {
            st.flutter_buf[ch][fw] = lr[ch];

            // Linear interpolation between ri and ri+1 samples back
            const int r0 = (fw + FLU_BUF - ri)     % FLU_BUF;
            const int r1 = (fw + FLU_BUF - ri - 1) % FLU_BUF;
            lr[ch] = st.flutter_buf[ch][r0] * (1.0f - rfrac)
                   + st.flutter_buf[ch][r1] * rfrac;
        }

        st.flutter_write = (st.flutter_write + 1u) % static_cast<uint32_t>(FLU_BUF);

        // HP wah sweep: cutoff 400 Hz ± 220 Hz at 0.5 Hz, mixed at 30% * p3.
        // Tonal thinning/thickening that's immediately audible on short hits.
        // s3/s4 unused by vinyl sim otherwise; DF2T HP biquad, Q = 0.707.
        {
            const float fc    = std::max(500.0f + p.p3 * 400.0f * lfo_slow, 40.0f);
            const float w0    = 2.0f * static_cast<float>(M_PI) * fc / sr;
            const float sinw  = std::sin(w0);
            const float cosw  = std::cos(w0);
            const float alpha = sinw * 0.7071f;
            const float a0i   = 1.0f / (1.0f + alpha);
            const float hb0   =  (1.0f + cosw) * 0.5f * a0i;
            const float hb1   = -(1.0f + cosw)         * a0i;
            const float hb2   =  (1.0f + cosw) * 0.5f * a0i;
            const float ha1   = -2.0f * cosw            * a0i;
            const float ha2   =  (1.0f - alpha)         * a0i;
            const float mix   = p.p3 * 0.45f;

            for (uint32_t ch = 0; ch < ch_count; ++ch) {
                const float x  = lr[ch];
                const float hp = hb0 * x + st.s3[ch];
                st.s3[ch] = hb1 * x - ha1 * hp + st.s4[ch];
                st.s4[ch] = hb2 * x - ha2 * hp;
                lr[ch] = x * (1.0f - mix) + hp * mix;
            }
        }
    }

    if (ch_count == 1)
        lr[1] = lr[0];
}

} // namespace sp303
