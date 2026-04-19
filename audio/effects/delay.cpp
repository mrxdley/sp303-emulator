#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── DELAY ────────────────────────────────────────────────────────────────────
//
// Bus effect — applied post-mix to all voices that have this effect enabled.
// Stereo ping-pong delay using a pre-allocated ring buffer in GlobalEffectState.
//
// p1 (tiM): delay time  30 ms – 750 ms
// p2 (Fdb): feedback    0 % – 90 %
// p3 (LEV): wet level   0 % – 100 %
//
// The ring buffer stores stereo interleaved f32.
// capacity_frames must be >= max delay frames before calling this function.

void delay_process_buffer(float* lr_interleaved, uint32_t frames,
                          const EffectParams& p, GlobalEffectState& st,
                          uint32_t sample_rate)
{
    if (st.buf.empty() || st.capacity_frames == 0) return;

    const float sr = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);

    // Delay time: 30 ms – 750 ms
    const float delay_sec    = 0.030f + p.p1 * 0.720f;
    const uint32_t delay_fr  = std::clamp(
        static_cast<uint32_t>(delay_sec * sr),
        1u, st.capacity_frames - 1u);

    const float feedback = p.p2 * 0.90f;   // 0 – 90 %
    const float level    = p.p3;            // 0 – 100 %
    const uint32_t cap   = st.capacity_frames;

    for (uint32_t f = 0; f < frames; ++f) {
        // Read echo from delay line
        const uint32_t read_pos = (st.write_pos + cap - delay_fr) % cap;
        const float echo_l = st.buf[read_pos * 2];
        const float echo_r = st.buf[read_pos * 2 + 1];

        const float in_l = lr_interleaved[f * 2];
        const float in_r = lr_interleaved[f * 2 + 1];

        // Write (input + feedback * echo) into delay line
        st.buf[st.write_pos * 2]     = in_l + feedback * echo_l;
        st.buf[st.write_pos * 2 + 1] = in_r + feedback * echo_r;

        // Output: pass dry signal through, add echoes
        lr_interleaved[f * 2]     = in_l + level * echo_l;
        lr_interleaved[f * 2 + 1] = in_r + level * echo_r;

        st.write_pos = (st.write_pos + 1) % cap;
    }
}

} // namespace sp303
