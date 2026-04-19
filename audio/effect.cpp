#include "effect.h"
#include "sp303.h"   // ButtonID constants

namespace sp303 {

// ─── Effect registry ──────────────────────────────────────────────────────────
//
// FX_DEFS index matches the value stored in Voice::fx_def_idx.
// Exactly one of process_frame / process_buffer is non-null per entry:
//   process_frame  → insert effect, per-voice per-sample
//   process_buffer → bus effect, post-mix on the wet bus

const EffectDef FX_DEFS[FX_SLOT_COUNT] = {
    // 0 — FILTER + DRIVE (insert)
    { "FILTER+DRIVE", "CoF", "rES", "drV", filter_drive_frame,    nullptr              },
    // 1 — PITCH (playback-rate based, handled in the voice playback path)
    { "PITCH",        "Pit", "Fdb", "dAL", nullptr,               nullptr              },
    // 1 — DELAY (bus)
    { "DELAY",        "tiM", "Fdb", "LEV", nullptr,               delay_process_buffer },
};

int fx_btn_to_index(int btn_id, int /*mfx_sub*/) {
    switch (btn_id) {
        case BTN_FILTER_DRIVE: return 0;
        case BTN_PITCH:        return 1;
        case BTN_DELAY:        return 2;
        // BTN_PITCH, BTN_MFX, BTN_VINYL_SIM, BTN_ISOLATOR: not yet implemented
        default:               return -1;
    }
}

} // namespace sp303
