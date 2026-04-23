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

void tape_echo_process_buffer(float* lr, uint32_t frames,
                              const EffectParams& p, GlobalEffectState& st,
                              uint32_t sample_rate)
{
    if (!lr || st.capacity_frames < 2 || st.buf.empty()) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float rate_hz = 0.18f + (1.0f - p.p1) * 0.22f;
    const float flutter_hz = 3.7f + (1.0f - p.p1) * 1.5f;
    const float delay_ms = 90.0f + (1.0f - p.p1) * 760.0f;
    const float feedback = std::clamp(p.p2, 0.0f, 1.0f) * 0.92f;
    const float wet = std::clamp(p.p3, 0.0f, 1.0f);
    const float lpf = 0.28f + (1.0f - p.p1) * 0.20f;

    const float base_delay = std::clamp(delay_ms * sr / 1000.0f, 1.0f, static_cast<float>(st.capacity_frames - 2));
    const float mod_depth = std::min(base_delay * 0.015f, 18.0f);

    const float p1_inc = (2.0f * static_cast<float>(M_PI) * rate_hz) / sr;
    const float p2_inc = (2.0f * static_cast<float>(M_PI) * flutter_hz) / sr;

    for (uint32_t f = 0; f < frames; ++f) {
        const float wow = std::sin(st.phase1) * mod_depth;
        const float flutter = std::sin(st.phase2) * (mod_depth * 0.35f);

        st.phase1 += p1_inc;
        st.phase2 += p2_inc;
        if (st.phase1 >= 2.0f * static_cast<float>(M_PI)) st.phase1 -= 2.0f * static_cast<float>(M_PI);
        if (st.phase2 >= 2.0f * static_cast<float>(M_PI)) st.phase2 -= 2.0f * static_cast<float>(M_PI);

        const float read_pos = static_cast<float>(st.write_pos) - base_delay - wow - flutter;
        const float echo_l = ring_read_linear(st, read_pos, 0);
        const float echo_r = ring_read_linear(st, read_pos, 1);

        st.feedback[0] += (echo_l - st.feedback[0]) * lpf;
        st.feedback[1] += (echo_r - st.feedback[1]) * lpf;

        const float in_l = lr[f * 2];
        const float in_r = lr[f * 2 + 1];

        st.buf[st.write_pos * 2]     = in_l + st.feedback[0] * feedback;
        st.buf[st.write_pos * 2 + 1] = in_r + st.feedback[1] * feedback;

        lr[f * 2]     = in_l + echo_l * wet;
        lr[f * 2 + 1] = in_r + echo_r * wet;

        st.write_pos = (st.write_pos + 1) % st.capacity_frames;
    }
}

} // namespace sp303
