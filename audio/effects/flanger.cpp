#include "../effect.h"

#include <algorithm>
#include <cmath>

namespace sp303 {

static inline float ring_read_linear(const GlobalEffectState& st, float frame_pos, uint32_t channel) {
    if (st.capacity_frames == 0 || st.buf.empty()) return 0.0f;
    const float capf = static_cast<float>(st.capacity_frames);
    while (frame_pos < 0.0f) frame_pos += capf;
    while (frame_pos >= capf) frame_pos -= capf;
    const uint32_t i0 = static_cast<uint32_t>(frame_pos);
    const uint32_t i1 = (i0 + 1) % st.capacity_frames;
    const float frac = frame_pos - static_cast<float>(i0);
    const float a = st.buf[i0 * 2 + channel];
    const float b = st.buf[i1 * 2 + channel];
    return a + (b - a) * frac;
}

void flanger_process_buffer(float* lr, uint32_t frames,
                            const EffectParams& p, GlobalEffectState& st,
                            uint32_t sample_rate)
{
    if (!lr || st.capacity_frames < 2 || st.buf.empty()) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float rate_knob = std::clamp(p.p2, 0.0f, 1.0f);
    const float rate_hz = (rate_knob <= 0.01f) ? 0.0f : (0.05f + rate_knob * 0.85f);
    const float resonance = std::clamp(p.p3, 0.0f, 1.0f) * 0.92f;
    const float manual_ms = 0.2f + std::clamp(p.p1, 0.0f, 1.0f) * 4.8f;
    const float base_delay = std::clamp(manual_ms * sr / 1000.0f, 1.0f, static_cast<float>(st.capacity_frames - 2));
    const float depth = (rate_hz > 0.0f) ? std::max(1.0f, base_delay * 0.85f) : 0.0f;
    const float phase_inc = (2.0f * static_cast<float>(M_PI) * rate_hz) / sr;

    for (uint32_t f = 0; f < frames; ++f) {
        const float lfo = (rate_hz > 0.0f) ? (0.5f + 0.5f * std::sin(st.phase1)) : 0.0f;
        st.phase1 += phase_inc;
        if (st.phase1 >= 2.0f * static_cast<float>(M_PI)) st.phase1 -= 2.0f * static_cast<float>(M_PI);

        const float delay = base_delay + depth * lfo;
        const float read_pos = static_cast<float>(st.write_pos) - delay;
        const float fb_l = ring_read_linear(st, read_pos, 0);
        const float fb_r = ring_read_linear(st, read_pos, 1);

        const float in_l = lr[f * 2];
        const float in_r = lr[f * 2 + 1];

        st.buf[st.write_pos * 2]     = in_l + fb_l * resonance;
        st.buf[st.write_pos * 2 + 1] = in_r + fb_r * resonance;

        lr[f * 2]     = in_l + (fb_l - in_l) * 0.7f;
        lr[f * 2 + 1] = in_r + (fb_r - in_r) * 0.7f;

        st.write_pos = (st.write_pos + 1) % st.capacity_frames;
    }
}

} // namespace sp303
