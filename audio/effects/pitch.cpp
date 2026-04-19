#include "../effect.h"

#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── PITCH ────────────────────────────────────────────────────────────────────
//
// Real SP-303 hardware uses a dedicated pitch algorithm.
// This emulator intentionally takes a simpler approach for now:
// the "pitch" effect is implemented by changing playback speed.
//
// Consequences:
// - pitch and duration change together
// - the texture will not match the hardware's algorithm
// - this is deliberate, and can be replaced later with a better shifter
//
// p1 (Pit): pitch range, -2 to +2 octaves
// p2 (Fdb): feedback amount, 0 % – 90 %
// p3 (dAL): direct/effect balance, 0 % – 100 % wet

float pitch_playback_rate(const EffectParams& p) {
    const float octaves = -2.0f + (std::clamp(p.p1, 0.0f, 1.0f) * 4.0f);
    return std::pow(2.0f, octaves);
}

float pitch_feedback_amount(const EffectParams& p) {
    return std::clamp(p.p2, 0.0f, 1.0f) * 0.90f;
}

float pitch_direct_effect_mix(const EffectParams& p) {
    return std::clamp(p.p3, 0.0f, 1.0f);
}

} // namespace sp303
