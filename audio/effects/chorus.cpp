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

void chorus_process_buffer(float* lr, uint32_t frames,
                           const EffectParams& p, GlobalEffectState& st,
                           uint32_t sample_rate)
{
    if (!lr || st.capacity_frames < 2 || st.buf.empty()) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float depth_ms = 1.5f + std::clamp(p.p1, 0.0f, 1.0f) * 10.5f;
    const float rate_hz = 0.08f + std::clamp(p.p2, 0.0f, 1.0f) * 2.4f;
    const float wet = std::clamp(p.p3, 0.0f, 1.0f) * 0.85f;
    const float base_ms = 13.0f;

    const float base_delay = std::clamp(base_ms * sr / 1000.0f, 1.0f, static_cast<float>(st.capacity_frames - 2));
    const float depth = std::min(depth_ms * sr / 1000.0f, static_cast<float>(st.capacity_frames / 4));
    const float phase_inc = (2.0f * static_cast<float>(M_PI) * rate_hz) / sr;

    for (uint32_t f = 0; f < frames; ++f) {
        const float lfo_l = std::sin(st.phase1);
        const float lfo_r = std::sin(st.phase1 + static_cast<float>(M_PI) * 0.5f);
        st.phase1 += phase_inc;
        if (st.phase1 >= 2.0f * static_cast<float>(M_PI)) st.phase1 -= 2.0f * static_cast<float>(M_PI);

        const float delay_l = base_delay + depth * lfo_l;
        const float delay_r = base_delay + depth * lfo_r;
        const float read_l = static_cast<float>(st.write_pos) - delay_l;
        const float read_r = static_cast<float>(st.write_pos) - delay_r;

        const float wet_l = ring_read_linear(st, read_l, 0);
        const float wet_r = ring_read_linear(st, read_r, 1);

        const float in_l = lr[f * 2];
        const float in_r = lr[f * 2 + 1];
        st.buf[st.write_pos * 2]     = in_l;
        st.buf[st.write_pos * 2 + 1] = in_r;

        lr[f * 2]     = in_l + (wet_l - in_l * 0.5f) * wet;
        lr[f * 2 + 1] = in_r + (wet_r - in_r * 0.5f) * wet;

        st.write_pos = (st.write_pos + 1) % st.capacity_frames;
    }
}

} // namespace sp303
