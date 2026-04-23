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
    { "FILTER+DRIVE", "CoF", "rES", "drV", filter_drive_frame,    nullptr               },
    // 1 — PITCH (playback-rate based, handled in the voice playback path)
    { "PITCH",        "Pit", "Fdb", "dAL", nullptr,               nullptr               },
    // 2 — DELAY (bus)
    { "DELAY",        "tiM", "Fdb", "LEV", nullptr,               delay_process_buffer  },
    // 3 — ISOLATOR (insert)
    { "ISOLATOR",     "Lo ", "Mid", "Hi ", isolator_frame,        nullptr               },
    // 4 — VINYL SIM (insert)
    { "VINYL SIM",    "CMP", "noS", "FLu", vinyl_sim_frame,       nullptr               },
    // 5 — MFX 1: REVERB (bus)
    { "REVERB",       "tiM", "ton", "LEV", nullptr,               reverb_process_buffer },
    // 6 — MFX 2: TAPE ECHO (bus)
    { "TAPE ECHO",    "rAt", "int", "LEV", nullptr,               tape_echo_process_buffer },
    // 7 — MFX 3: CHORUS (bus)
    { "CHORUS",       "dPt", "rAt", "LEV", nullptr,               chorus_process_buffer },
    // 8 — MFX 4: FLANGER (bus)
    { "FLANGER",      "dPt", "rAt", "rES", nullptr,               flanger_process_buffer },
    // 9 — MFX 5: PHASER (bus)
    { "PHASER",       "dPt", "rAt", "rES", nullptr,               phaser_process_buffer },
    // 10 — MFX 6: TREMOLO/PAN (bus)
    { "TRM/PAN",      "dPt", "rAt", "PAn", nullptr,               tremolo_pan_process_buffer },
};

int fx_btn_to_index(int btn_id, int mfx_sub) {
    switch (btn_id) {
        case BTN_FILTER_DRIVE: return 0;
        case BTN_PITCH:        return 1;
        case BTN_DELAY:        return 2;
        case BTN_ISOLATOR:     return 3;
        case BTN_VINYL_SIM:    return 4;
        case BTN_MFX:
            if (mfx_sub >= 0 && mfx_sub <= 5) return 5 + mfx_sub;
            return -1;
        default:               return -1;
    }
}

} // namespace sp303
