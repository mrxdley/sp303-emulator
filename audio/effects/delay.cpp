#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── DELAY ────────────────────────────────────────────────────────────────────
//
// Bus effect — applied post-mix to all voices that have this effect enabled.
// Stereo ping-pong delay with note-synced time (per SP-303 manual).
//
// p1 (note): selects 1 of 13 note values (0 = t32 .. 12 = t1)
//            time = note_beats * 60 / bpm, clamped to 1450 ms
// p2 (Fdb):  feedback  0 % – 90 %
// p3 (LEV):  wet level 0 % – 100 %
//
// BPM source (set by controller via GlobalEffectState::bpm):
//   pattern playing → pattern BPM
//   else            → BPM of longest delay-routed sample
//   fallback        → 120

// Note durations in quarter-note beats (ascending order, matching SP-303 manual)
static constexpr float NOTE_BEATS[13] = {
    0.125f,         // t32   — thirty-second note
    0.25f,          // t16   — sixteenth note
    0.375f,         // t16.  — dotted sixteenth
    1.0f / 3.0f,    // t8.3  — eighth-note triplet
    0.5f,           // t8    — eighth note
    0.75f,          // t8.   — dotted eighth
    2.0f / 3.0f,    // t4.3  — quarter-note triplet
    1.0f,           // t4    — quarter note
    1.5f,           // t4.   — dotted quarter
    4.0f / 3.0f,    // t2.3  — half-note triplet
    2.0f,           // t2    — half note
    3.0f,           // t2.   — dotted half
    4.0f,           // t1    — whole note
};

static constexpr float MAX_DELAY_SEC = 1.45f;

// Segment encoding (bit 7 = decimal point, bits 0-6 = segments a-g)
// Matches sp303 display bit layout: dp g f e d c b a
static constexpr uint8_t SEG_t   = 0x78;  // d e f g
static constexpr uint8_t SEG_BLK = 0x00;

// Segment bytes for each of the 13 note display values.
// Triplets: last digit = '3'. Dotted: dp on the note-digit.
static const uint8_t NOTE_DIG[13][3] = {
    { SEG_t, 0x4F, 0x5B },          // t32
    { SEG_t, 0x06, 0x7D },          // t16
    { SEG_t, 0x06, 0x7D | 0x80 },   // t16.  (dp on '6')
    { SEG_t, 0x7F | 0x80, 0x4F },   // t8.3  ('8.' '3')
    { SEG_t, 0x7F, SEG_BLK },       // t8
    { SEG_t, 0x7F | 0x80, SEG_BLK },// t8.   (dp on '8')
    { SEG_t, 0x66 | 0x80, 0x4F },   // t4.3  ('4.' '3')
    { SEG_t, 0x66, SEG_BLK },       // t4
    { SEG_t, 0x66 | 0x80, SEG_BLK },// t4.   (dp on '4')
    { SEG_t, 0x5B | 0x80, 0x4F },   // t2.3  ('2.' '3')
    { SEG_t, 0x5B, SEG_BLK },       // t2
    { SEG_t, 0x5B | 0x80, SEG_BLK },// t2.   (dp on '2')
    { SEG_t, 0x06, SEG_BLK },       // t1    ('1')
};

void delay_note_display(float p1, uint8_t out[3]) {
    int idx = std::clamp((int)std::lround(p1 * 12.0f), 0, 12);
    out[0] = NOTE_DIG[idx][0];
    out[1] = NOTE_DIG[idx][1];
    out[2] = NOTE_DIG[idx][2];
}

void delay_process_buffer(float* lr_interleaved, uint32_t frames,
                          const EffectParams& p, GlobalEffectState& st,
                          uint32_t sample_rate)
{
    if (st.buf.empty() || st.capacity_frames == 0) return;

    const float sr  = static_cast<float>(sample_rate > 0 ? sample_rate : 44100);
    const float bpm = st.bpm > 0.0f ? st.bpm : 120.0f;

    const int   note_idx   = std::clamp((int)std::lround(p.p1 * 12.0f), 0, 12);
    const float delay_sec  = std::min(NOTE_BEATS[note_idx] * 60.0f / bpm, MAX_DELAY_SEC);
    const uint32_t delay_fr = std::clamp(
        static_cast<uint32_t>(delay_sec * sr),
        1u, st.capacity_frames - 1u);

    const float feedback = p.p2 * 0.90f;
    const float level    = p.p3;
    const uint32_t cap   = st.capacity_frames;

    for (uint32_t f = 0; f < frames; ++f) {
        const uint32_t read_pos = (st.write_pos + cap - delay_fr) % cap;
        const float echo_l = st.buf[read_pos * 2];
        const float echo_r = st.buf[read_pos * 2 + 1];

        const float in_l = lr_interleaved[f * 2];
        const float in_r = lr_interleaved[f * 2 + 1];

        st.buf[st.write_pos * 2]     = in_l + feedback * echo_l;
        st.buf[st.write_pos * 2 + 1] = in_r + feedback * echo_r;

        lr_interleaved[f * 2]     = in_l + level * echo_l;
        lr_interleaved[f * 2 + 1] = in_r + level * echo_r;

        st.write_pos = (st.write_pos + 1) % cap;
    }
}

} // namespace sp303
