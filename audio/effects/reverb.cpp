#include "../effect.h"

#include <algorithm>

namespace sp303 {

// ─── REVERB (MFX 1) ───────────────────────────────────────────────────────────
//
// Bus effect. Schroeder reverb: 4 parallel feedback comb filters with LP
// damping in the feedback path, followed by 2 series allpass filters.
//
// p1 (tiM): reverb time  — feedback 0.50 – 0.95
// p2 (ton): tone         — LP damping inside comb (0 = bright, 1 = dark)
// p3 (LEV): wet level    — 0 – 1
//
// Delay line data packed mono into GlobalEffectState::buf (the existing
// sr*0.75 allocation holds ~66k floats; reverb needs ~6.5k at 48 kHz).
// aux_pos[0..5]     = write heads for comb[0..3] and allpass[0..1].
// aux_state[0..3][0] = LP damping filter state per comb.

static constexpr int N_COMBS = 4;
static constexpr int N_APS   = 2;

static constexpr uint32_t COMB_LEN44[N_COMBS] = { 1116, 1188, 1277, 1356 };
static constexpr uint32_t AP_LEN44[N_APS]     = {  556,  441 };

static uint32_t scale_delay(uint32_t len44, uint32_t sr) {
    return std::max(2u, static_cast<uint32_t>(len44 * static_cast<float>(sr) / 44100.0f + 0.5f));
}

// Fills offsets[] and lens[] for all 6 sections; returns total floats needed.
static uint32_t section_layout(uint32_t sr, uint32_t offsets[6], uint32_t lens[6]) {
    uint32_t off = 0;
    for (int i = 0; i < N_COMBS; ++i) {
        lens[i]    = scale_delay(COMB_LEN44[i], sr);
        offsets[i] = off;
        off       += lens[i];
    }
    for (int i = 0; i < N_APS; ++i) {
        lens[N_COMBS + i]    = scale_delay(AP_LEN44[i], sr);
        offsets[N_COMBS + i] = off;
        off                 += lens[N_COMBS + i];
    }
    return off;
}

void reverb_process_buffer(float* lr, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate)
{
    const uint32_t sr = sample_rate > 0 ? sample_rate : 44100;

    uint32_t offsets[6], lens[6];
    if (st.buf.size() < section_layout(sr, offsets, lens)) return;

    float* buf = st.buf.data();

    const float feedback = 0.5f + p.p1 * 0.45f;
    const float damp     = p.p2 * 0.85f;
    const float level    = p.p3;

    for (uint32_t f = 0; f < frames; ++f) {
        const float in_l  = lr[f * 2];
        const float in_r  = lr[f * 2 + 1];
        const float input = (in_l + in_r) * 0.5f;

        // 4 parallel feedback comb filters
        float comb_sum = 0.0f;
        for (int c = 0; c < N_COMBS; ++c) {
            float*         cbuf = buf + offsets[c];
            const uint32_t len  = lens[c];
            const uint32_t wp   = st.aux_pos[c];
            const float    del  = cbuf[wp];

            float& lp = st.aux_state[c][0];
            lp        = del * (1.0f - damp) + lp * damp;
            cbuf[wp]  = input + lp * feedback;
            st.aux_pos[c] = (wp + 1) % len;
            comb_sum += del;
        }
        comb_sum *= 0.25f;

        // 2 series allpass filters (g = 0.5)
        float ap = comb_sum;
        for (int a = 0; a < N_APS; ++a) {
            float*         abuf = buf + offsets[N_COMBS + a];
            const uint32_t len  = lens[N_COMBS + a];
            const uint32_t wp   = st.aux_pos[N_COMBS + a];
            const float    del  = abuf[wp];

            const float tmp = ap - 0.5f * del;
            abuf[wp]        = tmp;
            st.aux_pos[N_COMBS + a] = (wp + 1) % len;
            ap = 0.5f * tmp + del;
        }

        lr[f * 2]     = in_l + level * ap;
        lr[f * 2 + 1] = in_r + level * ap;
    }
}

} // namespace sp303
