#include "sp303_internal.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace sp303 {

// ─── Metadata tables ──────────────────────────────────────────────────────────

const ButtonDef BUTTON_DEFS[BTN_COUNT] = {
    { BTN_FILTER_DRIVE,    "FILTER+DRIVE",    nullptr,         nullptr       },
    { BTN_PITCH,           "PITCH",           nullptr,         nullptr       },
    { BTN_DELAY,           "DELAY",           nullptr,         nullptr       },
    { BTN_MFX,             "MFX",             nullptr,         nullptr       },
    { BTN_VINYL_SIM,       "VINYL SIM",       nullptr,         nullptr       },
    { BTN_ISOLATOR,        "ISOLATOR",        nullptr,         nullptr       },

    { BTN_START_END_LEVEL, "START/END/LEVEL", nullptr,         nullptr       },
    { BTN_TIME_BPM,        "TIME/BPM",        nullptr,         nullptr       },

    { BTN_PATTERN_SELECT,  "PATTERN SELECT",  nullptr,         nullptr       },
    { BTN_LENGTH,          "LENGTH",          nullptr,         nullptr       },
    { BTN_QUANTIZE,        "QUANTIZE",        nullptr,         nullptr       },

    { BTN_TAP_TEMPO,       "TAP TEMPO",       "EFFECT GRAB",   nullptr       },
    { BTN_CANCEL,          "CANCEL",          "PATTERN STOP",  nullptr       },
    { BTN_REMAIN,          "REMAIN",          "CURRENT PAD",   nullptr       },

    { BTN_LONG_LOFI,       "LONG",            "LOFI",          nullptr       },
    { BTN_STEREO,          "STEREO",          nullptr,         nullptr       },
    { BTN_GATE,            "GATE",            nullptr,         nullptr       },
    { BTN_LOOP,            "LOOP",            nullptr,         nullptr       },
    { BTN_REVERSE,         "REVERSE",         nullptr,         nullptr       },

    { BTN_DEL,             "DEL",             nullptr,         nullptr       },
    { BTN_REC,             "REC",             "PATTERN REC",   nullptr       },
    { BTN_RESAMPLE,        "RE-SAMPLE",       nullptr,         nullptr       },
    { BTN_MARK,            "MARK",            "START POINT",   "END POINT"   },

    { BTN_BANK_A,          "BANK A",          nullptr,         nullptr       },
    { BTN_BANK_B,          "BANK B",          nullptr,         nullptr       },
    { BTN_BANK_C,          "BANK C",          nullptr,         nullptr       },
    { BTN_BANK_D,          "BANK D",          nullptr,         nullptr       },

    // Pads — all 32 (banks A–D × 8).
    // Labels are "1"–"8" per bank; the renderer uses active_bank to pick the right set.
    { BTN_PAD_1,           "1",               nullptr,         nullptr       },
    { BTN_PAD_2,           "2",               nullptr,         nullptr       },
    { BTN_PAD_3,           "3",               nullptr,         nullptr       },
    { BTN_PAD_4,           "4",               nullptr,         nullptr       },
    { BTN_PAD_5,           "5",               nullptr,         nullptr       },
    { BTN_PAD_6,           "6",               nullptr,         nullptr       },
    { BTN_PAD_7,           "7",               nullptr,         nullptr       },
    { BTN_PAD_8,           "8",               nullptr,         nullptr       },
    { BTN_PAD_9,           "1",               nullptr,         nullptr       },
    { BTN_PAD_10,          "2",               nullptr,         nullptr       },
    { BTN_PAD_11,          "3",               nullptr,         nullptr       },
    { BTN_PAD_12,          "4",               nullptr,         nullptr       },
    { BTN_PAD_13,          "5",               nullptr,         nullptr       },
    { BTN_PAD_14,          "6",               nullptr,         nullptr       },
    { BTN_PAD_15,          "7",               nullptr,         nullptr       },
    { BTN_PAD_16,          "8",               nullptr,         nullptr       },
    { BTN_PAD_17,          "1",               nullptr,         nullptr       },
    { BTN_PAD_18,          "2",               nullptr,         nullptr       },
    { BTN_PAD_19,          "3",               nullptr,         nullptr       },
    { BTN_PAD_20,          "4",               nullptr,         nullptr       },
    { BTN_PAD_21,          "5",               nullptr,         nullptr       },
    { BTN_PAD_22,          "6",               nullptr,         nullptr       },
    { BTN_PAD_23,          "7",               nullptr,         nullptr       },
    { BTN_PAD_24,          "8",               nullptr,         nullptr       },
    { BTN_PAD_25,          "1",               nullptr,         nullptr       },
    { BTN_PAD_26,          "2",               nullptr,         nullptr       },
    { BTN_PAD_27,          "3",               nullptr,         nullptr       },
    { BTN_PAD_28,          "4",               nullptr,         nullptr       },
    { BTN_PAD_29,          "5",               nullptr,         nullptr       },
    { BTN_PAD_30,          "6",               nullptr,         nullptr       },
    { BTN_PAD_31,          "7",               nullptr,         nullptr       },
    { BTN_PAD_32,          "8",               nullptr,         nullptr       },

    { BTN_HOLD,            "HOLD",            nullptr,         nullptr       },
    { BTN_EXT_SOURCE,      "EXT SOURCE",      nullptr,         nullptr       },
};

const KnobDef KNOB_DEFS[KNOB_COUNT] = {
    { KNOB_VOLUME,    "VOLUME",    nullptr,  nullptr  },
    { KNOB_CUTOFF,    "CUTOFF",    "TIME",   "START"  },
    { KNOB_RESONANCE, "RESONANCE", "BPM",    "END"    },
    { KNOB_DRIVE,     "DRIVE",     "LEVEL",  nullptr  },
};

// ─── 7-Segment encoding ───────────────────────────────────────────────────────
//
// Bit layout:  7   6   5   4   3   2   1   0
//             dp   g   f   e   d   c   b   a

const uint8_t SEG_DIGITS[10] = {
    0x3F, // 0  a b c d e f
    0x06, // 1  b c
    0x5B, // 2  a b d e g
    0x4F, // 3  a b c d g
    0x66, // 4  b c f g
    0x6D, // 5  a c d f g
    0x7D, // 6  a c d e f g
    0x07, // 7  a b c
    0x7F, // 8  a b c d e f g
    0x6F, // 9  a b c d f g
};

const uint8_t SEG_BLANK = 0x00;
const uint8_t SEG_DASH  = SEG_G; // 0x40 — middle bar only

// "Err": E(0x79), r(0x50), r(0x50)
const uint8_t SEG_ERR[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
    SEG_E | SEG_G,                           // r
    SEG_E | SEG_G,                           // r
};

// "FuL" for memory full: F(0x71), u(0x1C), L(0x38)
const uint8_t SEG_FUL[3] = {
    SEG_A | SEG_E | SEG_F | SEG_G,           // F
    SEG_C | SEG_D | SEG_E,                   // u
    SEG_D | SEG_E | SEG_F,                   // L
};

// "rEC" for recording: r(0x50), E(0x79), C(0x39)
const uint8_t SEG_REC[3] = {
    SEG_E | SEG_G,                           // r
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
    SEG_A | SEG_D | SEG_E | SEG_F,          // C
};

// "rdY": r(0x50), d(0x5E), Y(0x6E)
const uint8_t SEG_RDY[3] = {
    SEG_E | SEG_G,                                    // r
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
    SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,           // Y
};

// "Edt": E(0x79), d(0x5E), t(0x78)
const uint8_t SEG_EDT[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
};

// "dEL": d(0x5E), E(0x79), L(0x38)
const uint8_t SEG_DEL[3] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
    SEG_D | SEG_E | SEG_F,                            // L
};

// "trC": t(0x78), r(0x50), C(0x39)
const uint8_t SEG_TRC[3] = {
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
    SEG_E | SEG_G,                                    // r
    SEG_A | SEG_D | SEG_E | SEG_F,                   // C
};

// "dAL": d(0x5E), A(0x77), L(0x38)
const uint8_t SEG_DAL[3] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                         // d
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,                 // A
    SEG_D | SEG_E | SEG_F,                                          // L
};

// "CHG": C(0x39), H(0x76), G(0x3D-ish lower-case style)
const uint8_t SEG_CHG[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F,                   // C
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,           // H
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F,           // G
};

// "LEV": L(0x38), E(0x79), V(~u-shape)
const uint8_t SEG_LEV[3] = {
    SEG_D | SEG_E | SEG_F,                            // L
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
    SEG_C | SEG_D | SEG_E,                           // V approximation
};

// "ErS": E, r, S
const uint8_t SEG_ERS[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
    SEG_E | SEG_G,                                   // r
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,           // S
};

// "FMt": F, M, t
const uint8_t SEG_FMT[3] = {
    SEG_A | SEG_E | SEG_F | SEG_G,                   // F
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,           // M approximation
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
};

// "EMP": E, M, P
const uint8_t SEG_EMP[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,           // M approximation
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,           // P
};

// "Prt": P, r, t
const uint8_t SEG_PRT[3] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,           // P
    SEG_E | SEG_G,                                   // r
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
};

// "Pit": P, i, t
const uint8_t SEG_PIT[3] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,           // P
    SEG_C,                                           // i approximation
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
};

// "Fdb": F, d, b
const uint8_t SEG_FDB[3] = {
    SEG_A | SEG_E | SEG_F | SEG_G,                   // F
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,           // b
};

// "oFF": o, F, F
const uint8_t SEG_OFF[3] = {
    SEG_C | SEG_D | SEG_E | SEG_G,                   // o
    SEG_A | SEG_E | SEG_F | SEG_G,                   // F
    SEG_A | SEG_E | SEG_F | SEG_G,                   // F
};

// "Ptn": P, t, n
const uint8_t SEG_PTN[3] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,           // P
    SEG_D | SEG_E | SEG_F | SEG_G,                   // t
    SEG_C | SEG_E | SEG_G                            // n approximation
};

// "CoT": C, o, T
const uint8_t SEG_COT[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F,                   // C
    SEG_C | SEG_D | SEG_E | SEG_G,                   // o
    SEG_D | SEG_E | SEG_F | SEG_G                    // T/t approximation
};

// "dRV": d, R, V
const uint8_t SEG_DRV[3] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
    SEG_E | SEG_G,                                   // R/r approximation
    SEG_C | SEG_D | SEG_E                            // V approximation
};

// "Lo ": L, o, blank
const uint8_t SEG_LO[3] = {
    SEG_D | SEG_E | SEG_F,                            // L
    SEG_C | SEG_D | SEG_E | SEG_G,                   // o
    SEG_BLANK,                                        // (space)
};

// "Mid": M (approximation), i, d
const uint8_t SEG_MID[3] = {
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,           // M — top bar + both uprights
    SEG_C,                                            // i
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
};

// "Hi ": H, i, blank
const uint8_t SEG_HI[3] = {
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,           // H
    SEG_C,                                            // i
    SEG_BLANK,                                        // (space)
};

void finish_threshold_recording(Device* dev);

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Device* create() {
    Device* dev = static_cast<Device*>(std::calloc(1, sizeof(Device)));
    if (!dev) return nullptr;

    std::memset(&dev->state, 0, sizeof(State));
    dev->sample_clock = 0;

    dev->state.knobs[KNOB_DRIVE].value = 0.8f;
    dev->state.knobs[KNOB_VOLUME].value = 1.0f;

    dev->state.display.digit[0] = SEG_BLANK;
    dev->state.display.digit[1] = SEG_BLANK;
    dev->state.display.digit[2] = SEG_BLANK;

    dev->state.active_bank = 0;
    dev->state.buttons[BTN_BANK_A].lit = true;

    dev->sampling_state = SAMPLING_IDLE;
    dev->sampling_target_pad = -1;
    dev->resample_source_pad = -1;
    dev->resample_dest_pad = -1;
    dev->last_sampling_target_pad = -1;
    dev->sampling_full = false;
    dev->sample_level_threshold = 5;
    dev->last_played_pad = -1;
    dev->edit_display_locked = false;
    dev->delete_target_pad = -1;
    dev->deleted_pad_pending = -1;
    dev->truncate_pad_pending = -1;
    dev->delete_all_group = -1;
    dev->delete_all_pending_group = -1;
    dev->swap_pad_a = -1;
    dev->swap_pad_b = -1;
    dev->swap_pending_a = -1;
    dev->swap_pending_b = -1;
    dev->memory_card_selected_bank = -1;
    dev->memory_card_backup_slot = -1;
    dev->memory_card_format_pending = false;
    dev->memory_card_sample_save_pending_slot = -1;
    dev->memory_card_sample_load_pending_slot = -1;
    dev->memory_card_dirty = false;
    dev->transient_display_frames = 0;
    dev->transient_display[0] = SEG_BLANK;
    dev->transient_display[1] = SEG_BLANK;
    dev->transient_display[2] = SEG_BLANK;
    dev->card_emp_lock = false;
    dev->card_emp_return_pattern = false;
    dev->card_emp_return_bank = 0;
    dev->card_emp_group = 0;
    dev->mark_edit_pad = -1;
    dev->mark_pending_action = 0;
    dev->mark_action_pad = -1;
    dev->mark_end_only_lock_pad = -1;
    dev->sampling_stereo = false;
    dev->sampling_quality = SAMPLE_QUALITY_STANDARD;
    dev->bpm_edit_mode = false;
    dev->bpm_value = -1;
    dev->bpm_armed_for_next_sample = false;
    dev->time_bpm_mode = false;
    dev->time_bpm_pad = -1;
    dev->pattern_mode = false;
    dev->pattern_bpm_edit_mode = false;
    dev->pattern_record_select = false;
    dev->pattern_recording = false;
    dev->pattern_erase_mode = false;
    dev->pattern_metronome_edit_mode = false;
    dev->pattern_length_edit_mode = false;
    dev->pattern_quantize_edit_mode = false;
    dev->pattern_delete_select = false;
    dev->pattern_delete_all_select = false;
    dev->pattern_delete_all_armed = false;
    dev->pattern_swap_select_a = false;
    dev->pattern_swap_select_b = false;
    dev->pattern_record_slot = -1;
    dev->pattern_delete_slot = -1;
    dev->pattern_delete_all_group = -1;
    dev->pattern_swap_a = -1;
    dev->pattern_swap_b = -1;
    dev->pattern_tap_display_value = 120;
    dev->pattern_tap_display_frames = 0;
    std::memset(dev->pattern_tap_times, 0, sizeof(dev->pattern_tap_times));
    dev->pattern_tap_count = 0;
    std::memset(dev->tap_times, 0, sizeof(dev->tap_times));
    dev->tap_count = 0;
    dev->pending_record_bpm_quantize = -1;
    dev->rec_gain_display_frames = 0;
    dev->rec_gain_value = 0;
    dev->resample_source_pad = -1;
    dev->resample_dest_pad = -1;

    std::memset(dev->pad_has_sample, 0, sizeof(dev->pad_has_sample));
    std::memset(dev->pad_led_hold_frames, 0, sizeof(dev->pad_led_hold_frames));
    std::memset(dev->pad_loop_mode, 0, sizeof(dev->pad_loop_mode));
    std::memset(dev->pad_gate_mode, 0, sizeof(dev->pad_gate_mode));
    std::memset(dev->pad_reverse_mode, 0, sizeof(dev->pad_reverse_mode));
    std::memset(dev->pad_is_marked, 0, sizeof(dev->pad_is_marked));

    dev->active_effect_btn = -1;
    std::memset(dev->pad_has_effect, 0, sizeof(dev->pad_has_effect));
    dev->mfx_type = 0;
    dev->mfx_knob_touched_while_pressed = false;
    pattern_init(&dev->pattern_seq);
    memory_card_reset(&dev->memory_card);

    dev->blink_phase = 0;
    dev->blink_on = false;

    return dev;
}

void destroy(Device* dev) {
    std::free(dev);
}

static void apply_effect_button_press(Device* dev, ButtonID btn) {
    if (!dev || !is_effect_btn(btn)) return;
    const int last_pad = dev->last_played_pad;
    const bool same_effect = (dev->active_effect_btn == (int)btn);

    if (dev->state.buttons[BTN_REMAIN].pressed) {
        dev->active_effect_btn = (int)btn;
        for (int i = 0; i < 32; ++i) dev->pad_has_effect[i] = true;
    } else if (last_pad >= 0 && last_pad < 32) {
        if (same_effect) {
            dev->pad_has_effect[last_pad] = !dev->pad_has_effect[last_pad];
            if (!dev->pad_has_effect[last_pad]) {
                dev->active_effect_btn = -1;
            }
        } else {
            dev->active_effect_btn = (int)btn;
            dev->pad_has_effect[last_pad] = true;
        }
    } else {
        dev->active_effect_btn = same_effect ? -1 : (int)btn;
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

void button_down(Device* dev, ButtonID btn) {
    if (!dev || btn < 0 || btn >= BTN_COUNT) return;
    dev->state.buttons[btn].pressed = true;
    if (btn == BTN_CANCEL || (btn >= BTN_BANK_A && btn <= BTN_BANK_D)) {
        std::printf("[INPUT] button_down btn=%d cancel=%d A=%d B=%d C=%d D=%d state=%d pattern_active=%d emp_lock=%d\n",
                    (int)btn,
                    dev->state.buttons[BTN_CANCEL].pressed ? 1 : 0,
                    dev->state.buttons[BTN_BANK_A].pressed ? 1 : 0,
                    dev->state.buttons[BTN_BANK_B].pressed ? 1 : 0,
                    dev->state.buttons[BTN_BANK_C].pressed ? 1 : 0,
                    dev->state.buttons[BTN_BANK_D].pressed ? 1 : 0,
                    (int)dev->sampling_state,
                    pattern_mode_active(dev) ? 1 : 0,
                    dev->card_emp_lock ? 1 : 0);
    }

    if (dev->card_emp_lock) {
        std::printf("[CARD] emp lock active, btn=%d\n", (int)btn);
        if (btn == BTN_CANCEL) {
            finish_card_emp_lock(dev);
        }
        return;
    }

    if (try_enter_pattern_card_workflow(dev, btn)) {
        return;
    }
    if (try_enter_sample_card_workflow(dev, btn)) {
        return;
    }

    if (dev->sampling_state == SAMPLING_EDIT) {
        int last_pad_btn = (dev->last_played_pad >= 0)
            ? (BTN_PAD_1 + dev->last_played_pad)
            : -1;

        if (btn == BTN_START_END_LEVEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->edit_display_locked = false;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }

        if (btn == last_pad_btn) {
            return;
        }

        dev->sampling_state = SAMPLING_IDLE;
        dev->edit_display_locked = false;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    }

    if (dev->sampling_state == SAMPLING_IDLE && dev->time_bpm_mode) {
        int time_pad_btn = (dev->time_bpm_pad >= 0)
            ? (BTN_PAD_1 + dev->time_bpm_pad)
            : -1;
        if (btn == BTN_TIME_BPM) {
            dev->time_bpm_mode = false;
            dev->time_bpm_pad = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (btn == time_pad_btn) {
            return;
        }
        dev->time_bpm_mode = false;
        dev->time_bpm_pad = -1;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    }

    if (btn == BTN_PATTERN_SELECT && dev->sampling_state == SAMPLING_IDLE) {
        if (dev->pattern_recording) {
            pattern_stop_record(&dev->pattern_seq);
            pattern_stop(&dev->pattern_seq);
            dev->pattern_recording = false;
            dev->pattern_record_slot = -1;
        }
        if (pattern_ui_active(dev)) {
            dev->pattern_mode = false;
            dev->pattern_record_select = false;
            clear_pattern_edit_modes(dev);
            clear_pattern_management_modes(dev);
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        } else {
            dev->pattern_mode = true;
            dev->pattern_record_select = false;
            dev->pattern_recording = false;
            dev->pattern_record_slot = -1;
            clear_pattern_edit_modes(dev);
            clear_pattern_management_modes(dev);
            display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
        }
        return;
    }

    if (pattern_mode_active(dev) && dev->sampling_state == SAMPLING_IDLE) {
        if (btn >= BTN_BANK_A && btn <= BTN_BANK_D) {
            uint8_t bank = static_cast<uint8_t>(btn - BTN_BANK_A);
            dev->state.active_bank = bank;
            for (int b = 0; b < 4; ++b)
                dev->state.buttons[BTN_BANK_A + b].lit = (b == bank);
            if (dev->pattern_delete_all_select || dev->pattern_delete_all_armed) {
                dev->pattern_delete_all_select = false;
                dev->pattern_delete_all_armed = true;
                dev->pattern_delete_all_group = (bank <= 1) ? 0 : 1;
            }
            return;
        }

        if (btn == BTN_CANCEL) {
            pattern_stop(&dev->pattern_seq);
            dev->pattern_mode = true;
            dev->pattern_record_select = false;
            dev->pattern_recording = false;
            dev->pattern_record_slot = -1;
            clear_pattern_edit_modes(dev);
            clear_pattern_management_modes(dev);
            display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            return;
        }

        if (dev->pattern_delete_all_select || dev->pattern_delete_all_armed) {
            if (btn == BTN_DEL && dev->pattern_delete_all_group >= 0) {
                int start = (dev->pattern_delete_all_group == 0) ? 0 : 16;
                pattern_clear_range(&dev->pattern_seq, start, start + 16);
                clear_pattern_management_modes(dev);
                dev->pattern_mode = true;
                display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            }
            return;
        }

        if (dev->pattern_swap_select_a || dev->pattern_swap_select_b) {
            if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
                int actual_slot = btn - BTN_PAD_1;
                if (dev->pattern_swap_select_a) {
                    dev->pattern_swap_a = actual_slot;
                    dev->pattern_swap_b = -1;
                    dev->pattern_swap_select_a = false;
                    dev->pattern_swap_select_b = true;
                } else {
                    dev->pattern_swap_b = actual_slot;
                }
                return;
            }
            if (btn == BTN_REC &&
                dev->pattern_swap_select_b &&
                dev->pattern_swap_a >= 0 &&
                dev->pattern_swap_b >= 0 &&
                dev->pattern_swap_a != dev->pattern_swap_b) {
                pattern_swap_slots(&dev->pattern_seq, dev->pattern_swap_a, dev->pattern_swap_b);
                clear_pattern_management_modes(dev);
                dev->pattern_mode = true;
                display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            }
            return;
        }

        if (dev->pattern_delete_select) {
            if (btn == BTN_REC && dev->state.buttons[BTN_DEL].pressed) {
                clear_pattern_edit_modes(dev);
                clear_pattern_management_modes(dev);
                dev->pattern_swap_select_a = true;
                dev->pattern_mode = true;
                return;
            }
            if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
                int actual_slot = btn - BTN_PAD_1;
                if (pattern_has_data(&dev->pattern_seq, actual_slot)) {
                    dev->pattern_delete_slot = actual_slot;
                }
                return;
            }
            if (btn == BTN_DEL && dev->pattern_delete_slot >= 0) {
                pattern_clear_slot(&dev->pattern_seq, dev->pattern_delete_slot);
                clear_pattern_management_modes(dev);
                dev->pattern_mode = true;
                display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            }
            return;
        }

        if (dev->pattern_erase_mode) {
            if (btn == BTN_DEL) {
                clear_pattern_management_modes(dev);
            } else if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
                int sample_slot = btn - BTN_PAD_1;
                pattern_set_erase_pad(&dev->pattern_seq, sample_slot, true);
            }
            return;
        }

        if (btn == BTN_DEL) {
            clear_pattern_edit_modes(dev);
            if (dev->pattern_recording) {
                dev->pattern_erase_mode = true;
                return;
            }
            if (dev->state.buttons[BTN_CANCEL].pressed) {
                clear_pattern_management_modes(dev);
                dev->pattern_delete_all_select = true;
                dev->pattern_mode = true;
                return;
            }
            clear_pattern_management_modes(dev);
            dev->pattern_delete_select = true;
            dev->pattern_delete_slot = -1;
            dev->pattern_mode = true;
            return;
        }

        if (btn == BTN_REC && dev->state.buttons[BTN_DEL].pressed && !dev->pattern_recording) {
            clear_pattern_edit_modes(dev);
            clear_pattern_management_modes(dev);
            dev->pattern_swap_select_a = true;
            dev->pattern_mode = true;
            return;
        }

        if (btn == BTN_TIME_BPM && !dev->pattern_recording) {
            dev->pattern_bpm_edit_mode = !dev->pattern_bpm_edit_mode;
            dev->pattern_metronome_edit_mode = false;
            dev->pattern_length_edit_mode = false;
            dev->pattern_quantize_edit_mode = false;
            dev->pattern_tap_count = 0;
            return;
        }

        if (btn == BTN_START_END_LEVEL) {
            int target_slot = dev->pattern_record_slot;
            if (target_slot < 0 && pattern_is_playing(&dev->pattern_seq)) {
                target_slot = pattern_get_current_slot(&dev->pattern_seq);
            }
            if (target_slot < 0) {
                for (int i = 0; i < 32; ++i) {
                    if (pattern_has_data(&dev->pattern_seq, i)) {
                        target_slot = i;
                        break;
                    }
                }
            }
            if (target_slot >= 0) {
                dev->pattern_record_slot = target_slot;
                dev->pattern_metronome_edit_mode = !dev->pattern_metronome_edit_mode;
                dev->pattern_bpm_edit_mode = false;
                dev->pattern_length_edit_mode = false;
                dev->pattern_quantize_edit_mode = false;
            }
            return;
        }

        if (btn == BTN_LENGTH && dev->pattern_record_select && dev->pattern_record_slot >= 0) {
            dev->pattern_length_edit_mode = !dev->pattern_length_edit_mode;
            dev->pattern_metronome_edit_mode = false;
            dev->pattern_bpm_edit_mode = false;
            dev->pattern_quantize_edit_mode = false;
            return;
        }

        if (btn == BTN_QUANTIZE && dev->pattern_record_slot >= 0) {
            dev->pattern_quantize_edit_mode = !dev->pattern_quantize_edit_mode;
            dev->pattern_metronome_edit_mode = false;
            dev->pattern_bpm_edit_mode = false;
            dev->pattern_length_edit_mode = false;
            return;
        }

        if (btn == BTN_TAP_TEMPO) {
            tap_record(dev->pattern_tap_times, &dev->pattern_tap_count, dev->sample_clock);
            if (dev->pattern_tap_count >= 4) {
                uint32_t d1 = dev->pattern_tap_times[1] - dev->pattern_tap_times[0];
                uint32_t d2 = dev->pattern_tap_times[2] - dev->pattern_tap_times[1];
                uint32_t d3 = dev->pattern_tap_times[3] - dev->pattern_tap_times[2];
                float avg = (d1 + d2 + d3) / 3.0f;
                if (avg > 1.0f) {
                    int bpm = (int)std::lround((60.0f * CORE_TAP_SAMPLE_RATE) / avg);
                    pattern_set_bpm(&dev->pattern_seq, bpm);
                    dev->pattern_tap_display_value = pattern_get_bpm(&dev->pattern_seq);
                    dev->pattern_tap_display_frames = 180;
                }
            }
            return;
        }

        if (btn == BTN_REC) {
            if (dev->pattern_recording) {
                pattern_stop_record(&dev->pattern_seq);
                pattern_stop(&dev->pattern_seq);
                dev->pattern_recording = false;
                dev->pattern_record_slot = -1;
                dev->pattern_mode = true;
                dev->pattern_record_select = false;
                clear_pattern_edit_modes(dev);
                clear_pattern_management_modes(dev);
                display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            } else if (dev->pattern_record_select && dev->pattern_record_slot >= 0) {
                pattern_start_record(&dev->pattern_seq, dev->pattern_record_slot);
                dev->pattern_recording = true;
                dev->pattern_record_select = false;
                dev->pattern_mode = true;
                clear_pattern_edit_modes(dev);
                clear_pattern_management_modes(dev);
            } else {
                pattern_stop(&dev->pattern_seq);
                clear_pattern_management_modes(dev);
                dev->pattern_record_select = true;
                dev->pattern_record_slot = -1;
                dev->pattern_mode = true;
                clear_pattern_edit_modes(dev);
            }
            return;
        }

        if (!dev->pattern_recording && btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
            if (!pattern_ui_active(dev) && pattern_is_playing(&dev->pattern_seq)) {
                return;
            }
            int actual_slot = btn - BTN_PAD_1;

            if (dev->pattern_record_select) {
                dev->pattern_record_slot = actual_slot;
                return;
            }

            if (pattern_has_data(&dev->pattern_seq, actual_slot)) {
                if (pattern_is_playing(&dev->pattern_seq) &&
                    pattern_get_current_slot(&dev->pattern_seq) == actual_slot) {
                    pattern_stop(&dev->pattern_seq);
                    dev->pattern_mode = true;
                } else {
                    pattern_start_playback(&dev->pattern_seq, actual_slot);
                    dev->pattern_mode = false;
                }
            }
            return;
        }
    }

    if (dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_SELECT ||
        dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_ARMED ||
        dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_SELECT ||
        dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_ARMED) {
        if (btn == BTN_CANCEL) {
            exit_card_workflow(dev, true);
            return;
        }
        if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
            int backup_slot = (btn - BTN_PAD_1) % 8;
            if ((dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_SELECT ||
                 dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_ARMED) &&
                get_memory_card_backup_kind(dev, backup_slot) != MEMORY_CARD_BACKUP_PATTERNS) {
                return;
            }
            dev->memory_card_backup_slot = backup_slot;
            if (dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_SELECT) {
                dev->sampling_state = SAMPLING_CARD_PATTERN_SAVE_ARMED;
            } else if (dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_SELECT) {
                dev->sampling_state = SAMPLING_CARD_PATTERN_LOAD_ARMED;
            }
            return;
        }
        if (btn == BTN_REC && dev->memory_card_backup_slot >= 0) {
            if (dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_ARMED) {
                save_patterns_to_backup(dev, dev->memory_card_backup_slot);
            } else if (dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_ARMED) {
                load_patterns_from_backup(dev, dev->memory_card_backup_slot);
            }
            exit_card_workflow(dev, true);
            return;
        }
        return;
    }

    if (dev->sampling_state == SAMPLING_MARK_EDIT ||
        dev->sampling_state == SAMPLING_MARK_END_ONLY) {
        int actual_pad = button_to_actual_pad(dev, btn);
        if (dev->sampling_state == SAMPLING_MARK_EDIT &&
            dev->state.buttons[BTN_MARK].pressed &&
            actual_pad >= 0 && actual_pad < 32) {
            if (dev->pad_has_sample[actual_pad]) {
                std::printf("[MARK] held-pad switch -> bank %c pad %d, from BOTH to END_ONLY\n",
                            pad_bank_name(actual_pad), pad_bank_number(actual_pad));
                dev->mark_pending_action = 0;
                dev->mark_action_pad = -1;
                dev->mark_edit_pad = actual_pad;
                dev->last_played_pad = actual_pad;
                dev->mark_end_only_lock_pad = actual_pad;
                dev->sampling_state = SAMPLING_MARK_END_ONLY;
                return;
            }
        }
        if (btn == BTN_CANCEL) {
            std::printf("[MARK] cancel -> mode=%s pad=%d lock=%d\n",
                        dev->sampling_state == SAMPLING_MARK_EDIT ? "BOTH" : "END_ONLY",
                        dev->mark_edit_pad,
                        dev->mark_end_only_lock_pad);
            dev->sampling_state = SAMPLING_IDLE;
            dev->mark_edit_pad = -1;
            dev->mark_pending_action = 0;
            dev->mark_action_pad = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (dev->sampling_state == SAMPLING_MARK_EDIT) {
            if (btn == BTN_MARK) {
                std::printf("[MARK] queue END from BOTH -> bank %c pad %d\n",
                            pad_bank_name(dev->mark_edit_pad), pad_bank_number(dev->mark_edit_pad));
                dev->mark_pending_action = 2;
                dev->mark_action_pad = dev->mark_edit_pad;
                dev->sampling_state = SAMPLING_IDLE;
                dev->mark_edit_pad = -1;
                return;
            }
            if (actual_pad == dev->mark_edit_pad) {
                std::printf("[MARK] same-pad release/press exits BOTH -> bank %c pad %d\n",
                            pad_bank_name(dev->mark_edit_pad), pad_bank_number(dev->mark_edit_pad));
                dev->sampling_state = SAMPLING_IDLE;
                dev->mark_edit_pad = -1;
                return;
            }
        } else {
            if (btn == BTN_MARK) {
                std::printf("[MARK] queue END from END_ONLY -> bank %c pad %d\n",
                            pad_bank_name(dev->mark_edit_pad), pad_bank_number(dev->mark_edit_pad));
                dev->mark_pending_action = 2;
                dev->mark_action_pad = dev->mark_edit_pad;
                dev->sampling_state = SAMPLING_IDLE;
                dev->mark_edit_pad = -1;
                return;
            }
        }
    }

    if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        if (btn == BTN_DEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->delete_target_pad = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (btn == BTN_MARK) {
            if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
                dev->pad_has_sample[dev->last_played_pad] &&
                dev->pad_is_marked[dev->last_played_pad]) {
                dev->sampling_state = SAMPLING_TRUNCATE_ARMED;
                display_raw(dev, SEG_TRC[0], SEG_TRC[1], SEG_TRC[2]);
            }
            return;
        }
        int actual_pad = button_to_actual_pad(dev, btn);
        if (actual_pad >= 0 && actual_pad < 32) {
            if (dev->pad_has_sample[actual_pad]) {
                dev->delete_target_pad = actual_pad;
                dev->sampling_state = SAMPLING_DELETE_ARMED;
            }
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        dev->delete_target_pad = -1;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_DELETE_ALL_SELECT ||
               dev->sampling_state == SAMPLING_DELETE_ALL_ARMED) {
        if (btn >= BTN_BANK_A && btn <= BTN_BANK_D) {
            dev->delete_all_group = (btn <= BTN_BANK_B) ? 0 : 1;
            dev->sampling_state = SAMPLING_DELETE_ALL_ARMED;
            return;
        }
        if (btn == BTN_DEL &&
            dev->sampling_state == SAMPLING_DELETE_ALL_ARMED &&
            dev->delete_all_group >= 0) {
            int start = (dev->delete_all_group == 0) ? 0 : 16;
            int end = start + 16;
            for (int i = start; i < end; ++i) {
                dev->pad_has_sample[i] = false;
                dev->pad_led_hold_frames[i] = 0;
                dev->pad_loop_mode[i] = false;
                dev->pad_gate_mode[i] = false;
                dev->pad_reverse_mode[i] = false;
            }
            if (dev->last_played_pad >= start && dev->last_played_pad < end) {
                dev->last_played_pad = -1;
            }
            dev->delete_all_pending_group = dev->delete_all_group;
            dev->delete_all_group = -1;
            dev->sampling_state = SAMPLING_IDLE;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        return;
    } else if (dev->sampling_state == SAMPLING_TRUNCATE_ARMED) {
        if (btn == BTN_DEL) {
            if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
                dev->pad_has_sample[dev->last_played_pad] &&
                dev->pad_is_marked[dev->last_played_pad]) {
                dev->truncate_pad_pending = dev->last_played_pad;
            }
            dev->sampling_state = SAMPLING_IDLE;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_DELETE_ARMED) {
        if (btn == BTN_DEL) {
            if (dev->delete_target_pad >= 0 && dev->delete_target_pad < 32) {
                dev->pad_has_sample[dev->delete_target_pad] = false;
                dev->pad_loop_mode[dev->delete_target_pad] = false;
                dev->pad_gate_mode[dev->delete_target_pad] = false;
                dev->pad_reverse_mode[dev->delete_target_pad] = false;
                dev->pad_led_hold_frames[dev->delete_target_pad] = 0;
                dev->pad_is_playing[dev->delete_target_pad] = false;
                if (dev->last_played_pad == dev->delete_target_pad) {
                    dev->last_played_pad = -1;
                }
                dev->deleted_pad_pending = dev->delete_target_pad;
            }
            dev->sampling_state = SAMPLING_IDLE;
            dev->delete_target_pad = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        dev->delete_target_pad = -1;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_SWAP_SELECT_A ||
               dev->sampling_state == SAMPLING_SWAP_SELECT_B) {
        if (btn >= BTN_BANK_A && btn <= BTN_BANK_D) {
            uint8_t bank = static_cast<uint8_t>(btn - BTN_BANK_A);
            dev->state.active_bank = bank;
            restore_active_bank_lights(dev);
            return;
        }
        int actual_pad = button_to_actual_pad(dev, btn);
        if (actual_pad >= 0 && actual_pad < 32) {
            if (dev->sampling_state == SAMPLING_SWAP_SELECT_A) {
                dev->swap_pad_a = actual_pad;
                dev->swap_pad_b = -1;
                dev->sampling_state = SAMPLING_SWAP_SELECT_B;
            } else {
                dev->swap_pad_b = actual_pad;
            }
            return;
        }
        if (btn == BTN_REC &&
            dev->sampling_state == SAMPLING_SWAP_SELECT_B &&
            dev->swap_pad_a >= 0 && dev->swap_pad_b >= 0 &&
            dev->swap_pad_a != dev->swap_pad_b) {
            int a = dev->swap_pad_a;
            int b = dev->swap_pad_b;
            std::swap(dev->pad_has_sample[a], dev->pad_has_sample[b]);
            std::swap(dev->pad_loop_mode[a], dev->pad_loop_mode[b]);
            std::swap(dev->pad_gate_mode[a], dev->pad_gate_mode[b]);
            std::swap(dev->pad_reverse_mode[a], dev->pad_reverse_mode[b]);
            std::swap(dev->pad_led_hold_frames[a], dev->pad_led_hold_frames[b]);
            if (dev->last_played_pad == a) dev->last_played_pad = b;
            else if (dev->last_played_pad == b) dev->last_played_pad = a;
            if (dev->last_sampling_target_pad == a) dev->last_sampling_target_pad = b;
            else if (dev->last_sampling_target_pad == b) dev->last_sampling_target_pad = a;

            dev->swap_pending_a = a;
            dev->swap_pending_b = b;
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            dev->sampling_state = SAMPLING_IDLE;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (btn == BTN_CANCEL) {
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            dev->sampling_state = SAMPLING_IDLE;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        return;
    } else if (dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
               dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
               dev->sampling_state == SAMPLING_RESAMPLE_ARMED ||
               dev->sampling_state == SAMPLING_RESAMPLE_RECORDING) {
        if (btn == BTN_CANCEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->sampling_target_pad = -1;
            dev->resample_source_pad = -1;
            dev->resample_dest_pad = -1;
            dev->sampling_full = false;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (btn == BTN_RESAMPLE && dev->sampling_state != SAMPLING_RESAMPLE_RECORDING) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->sampling_target_pad = -1;
            dev->resample_source_pad = -1;
            dev->resample_dest_pad = -1;
            dev->sampling_full = false;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }

        if (dev->sampling_state == SAMPLING_RESAMPLE_SOURCE) {
            if (btn == BTN_REC) {
                dev->sampling_state = SAMPLING_RESAMPLE_DEST;
                dev->resample_dest_pad = -1;
                dev->sampling_target_pad = -1;
                dev->rec_gain_display_frames = 0;
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                return;
            }
        } else if (dev->sampling_state == SAMPLING_RESAMPLE_DEST) {
            if (btn == BTN_REC && dev->resample_dest_pad >= 0) {
                dev->sampling_state = SAMPLING_RESAMPLE_ARMED;
                dev->rec_gain_display_frames = 0;
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                return;
            }
            int actual_pad = button_to_actual_pad(dev, btn);
            if (actual_pad >= 0 && actual_pad < 32) {
                dev->resample_dest_pad = actual_pad;
                dev->sampling_target_pad = actual_pad;
                return;
            }
        } else if (dev->sampling_state == SAMPLING_RESAMPLE_ARMED) {
            int actual_pad = button_to_actual_pad(dev, btn);
            if (actual_pad >= 0 && actual_pad < 32) {
                if (dev->pad_has_sample[actual_pad] && actual_pad != dev->resample_dest_pad) {
                    dev->resample_source_pad = actual_pad;
                    dev->last_played_pad = actual_pad;
                    dev->pad_led_hold_frames[actual_pad] = 180;
                    dev->sampling_state = SAMPLING_RESAMPLE_RECORDING;
                    dev->sampling_target_pad = dev->resample_dest_pad;
                    dev->sampling_full = false;
                }
                return;
            }
        } else if (dev->sampling_state == SAMPLING_RESAMPLE_RECORDING) {
            if (btn == BTN_REC) {
                finish_threshold_recording(dev);
                return;
            }
        }
    } else if (dev->sampling_state == SAMPLING_CARD_FORMAT_SELECT ||
               dev->sampling_state == SAMPLING_CARD_FORMAT_ARMED ||
               dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_SELECT ||
               dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_ARMED ||
               dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_SELECT ||
               dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_ARMED) {
        if (btn == BTN_CANCEL) {
            exit_card_workflow(dev, false);
            return;
        }
        if (dev->sampling_state == SAMPLING_CARD_FORMAT_SELECT ||
            dev->sampling_state == SAMPLING_CARD_FORMAT_ARMED) {
            if (btn == BTN_BANK_C || btn == BTN_BANK_D) {
                dev->memory_card_selected_bank = (int)btn;
                dev->sampling_state = SAMPLING_CARD_FORMAT_ARMED;
                return;
            }
            if (btn == BTN_DEL &&
                dev->sampling_state == SAMPLING_CARD_FORMAT_ARMED &&
                dev->memory_card_selected_bank >= 0) {
                memory_card_format(&dev->memory_card);
                dev->memory_card_dirty = true;
                for (int i = 16; i < 32; ++i) {
                    pad_clear_sample(dev, i);
                    dev->pad_led_hold_frames[i] = 0;
                    dev->pad_is_playing[i] = false;
                }
                pattern_clear_range(&dev->pattern_seq, 16, 32);
                if (dev->last_played_pad >= 16) dev->last_played_pad = -1;
                if (dev->last_sampling_target_pad >= 16) dev->last_sampling_target_pad = -1;
                dev->delete_all_pending_group = 1;
                dev->memory_card_format_pending = true;
                exit_card_workflow(dev, false);
                return;
            }
            return;
        }

        if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
            int backup_slot = (btn - BTN_PAD_1) % 8;
            if ((dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_SELECT ||
                 dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_ARMED) &&
                get_memory_card_backup_kind(dev, backup_slot) != MEMORY_CARD_BACKUP_SAMPLES) {
                return;
            }
            dev->memory_card_backup_slot = backup_slot;
            if (dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_SELECT) {
                dev->sampling_state = SAMPLING_CARD_SAMPLE_SAVE_ARMED;
            } else if (dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_SELECT) {
                dev->sampling_state = SAMPLING_CARD_SAMPLE_LOAD_ARMED;
            }
            return;
        }
        if (btn == BTN_REC && dev->memory_card_backup_slot >= 0) {
            if (dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_ARMED) {
                dev->memory_card_sample_save_pending_slot = dev->memory_card_backup_slot;
            } else if (dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_ARMED) {
                dev->memory_card_sample_load_pending_slot = dev->memory_card_backup_slot;
            }
            exit_card_workflow(dev, false);
            return;
        }
        return;
    }

    if (btn >= BTN_BANK_A && btn <= BTN_BANK_D) {
        uint8_t bank = static_cast<uint8_t>(btn - BTN_BANK_A);
        dev->state.active_bank = bank;
        for (int b = 0; b < 4; ++b)
            dev->state.buttons[BTN_BANK_A + b].lit = (b == bank);
        return;
    }

    if (btn == BTN_TIME_BPM) {
        if (dev->sampling_state == SAMPLING_STANDBY) {
            dev->bpm_edit_mode = !dev->bpm_edit_mode;
            dev->tap_count = 0;
            if (!dev->bpm_edit_mode) {
                dev->bpm_armed_for_next_sample = (dev->bpm_value >= 40 && dev->bpm_value <= 200);
            }
        } else if (dev->sampling_state == SAMPLING_IDLE &&
                   !pattern_mode_active(dev) &&
                   dev->last_played_pad >= 0 &&
                   dev->last_played_pad < 32 &&
                   dev->pad_has_sample[dev->last_played_pad]) {
            dev->time_bpm_mode = !dev->time_bpm_mode;
            dev->time_bpm_pad = dev->time_bpm_mode ? dev->last_played_pad : -1;
        }
        return;
    }

    if (btn == BTN_MARK) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad] &&
            dev->pad_is_playing[dev->last_played_pad]) {
            if (dev->mark_end_only_lock_pad == dev->last_played_pad) {
                if (dev->state.buttons[BTN_MARK].lit &&
                    dev->pad_is_playing[dev->last_played_pad]) {
                    std::printf("[MARK] reset full while END_ONLY locked -> bank %c pad %d\n",
                                pad_bank_name(dev->last_played_pad),
                                pad_bank_number(dev->last_played_pad));
                    dev->mark_pending_action = 3;
                    dev->mark_action_pad = dev->last_played_pad;
                    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                }
                else {
                    std::printf("[MARK] suppress BOTH path because END_ONLY lock active -> bank %c pad %d\n",
                                pad_bank_name(dev->last_played_pad),
                                pad_bank_number(dev->last_played_pad));
                }
                return;
            }
            if (dev->state.buttons[BTN_MARK].lit &&
                dev->pad_is_playing[dev->last_played_pad]) {
                std::printf("[MARK] reset full -> bank %c pad %d\n",
                            pad_bank_name(dev->last_played_pad),
                            pad_bank_number(dev->last_played_pad));
                dev->mark_pending_action = 3;
                dev->mark_action_pad = dev->last_played_pad;
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                return;
            }
            std::printf("[MARK] enter BOTH -> bank %c pad %d\n",
                        pad_bank_name(dev->last_played_pad),
                        pad_bank_number(dev->last_played_pad));
            dev->sampling_state = SAMPLING_MARK_EDIT;
            dev->mark_edit_pad = dev->last_played_pad;
            dev->mark_pending_action = 1;
            dev->mark_action_pad = dev->mark_edit_pad;
            std::printf("[MARK] queue START from BOTH -> bank %c pad %d\n",
                        pad_bank_name(dev->mark_edit_pad),
                        pad_bank_number(dev->mark_edit_pad));
        }
        return;
    }

    if (btn == BTN_TAP_TEMPO &&
        dev->sampling_state == SAMPLING_STANDBY &&
        dev->bpm_edit_mode) {
        tap_record(dev->tap_times, &dev->tap_count, dev->sample_clock);
        if (dev->tap_count >= 4) {
            uint32_t d1 = dev->tap_times[1] - dev->tap_times[0];
            uint32_t d2 = dev->tap_times[2] - dev->tap_times[1];
            uint32_t d3 = dev->tap_times[3] - dev->tap_times[2];
            float avg = (d1 + d2 + d3) / 3.0f;
            if (avg > 1.0f) {
                int bpm = (int)std::lround((60.0f * CORE_TAP_SAMPLE_RATE) / avg);
                dev->bpm_value = std::clamp(bpm, 40, 200);
            }
        }
        return;
    }

    // ─── REC button ───────────────────────────────────────────────────────────
    if (btn == BTN_REC) {
        if (dev->sampling_state == SAMPLING_THRESHOLD) {
            dev->sampling_state = SAMPLING_IDLE;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }

        if ((dev->sampling_state == SAMPLING_IDLE || dev->sampling_state == SAMPLING_DELETE_SELECT) &&
            dev->state.buttons[BTN_DEL].pressed) {
            dev->sampling_state = SAMPLING_SWAP_SELECT_A;
            dev->delete_target_pad = -1;
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            display_raw(dev, SEG_CHG[0], SEG_CHG[1], SEG_CHG[2]);
            return;
        }

        if (dev->sampling_state == SAMPLING_IDLE) {
            if (dev->state.buttons[BTN_CANCEL].pressed) {
                dev->sampling_state = SAMPLING_THRESHOLD;
                return;
            }
            dev->sampling_state = SAMPLING_STANDBY;
            dev->sampling_target_pad = -1;
            dev->sampling_full = false;
            dev->bpm_edit_mode = false;
            dev->tap_count = 0;
            dev->bpm_armed_for_next_sample = false;
        } else if (dev->sampling_state == SAMPLING_STANDBY) {
            if (dev->bpm_edit_mode) {
                dev->bpm_edit_mode = false;
                dev->tap_count = 0;
                dev->bpm_armed_for_next_sample = (dev->bpm_value >= 40 && dev->bpm_value <= 200);
            }
            if (dev->sampling_target_pad >= 0) {
                dev->sampling_state = SAMPLING_READY;
            }
        } else if (dev->sampling_state == SAMPLING_READY) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->sampling_target_pad = -1;
            dev->sampling_full = false;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        } else if (dev->sampling_state == SAMPLING_RECORDING) {
            finish_threshold_recording(dev);
        }
        return;
    }

    if (btn == BTN_DEL) {
        if (dev->sampling_state == SAMPLING_IDLE) {
            if (dev->state.buttons[BTN_CANCEL].pressed) {
                dev->sampling_state = SAMPLING_DELETE_ALL_SELECT;
                dev->delete_all_group = -1;
                display_raw(dev, SEG_DAL[0], SEG_DAL[1], SEG_DAL[2]);
                return;
            }
            dev->sampling_state = SAMPLING_DELETE_SELECT;
            dev->delete_target_pad = -1;
            display_raw(dev, SEG_DEL[0], SEG_DEL[1], SEG_DEL[2]);
        }
        return;
    }

    if (btn == BTN_RESAMPLE) {
        if (dev->sampling_state == SAMPLING_IDLE) {
            dev->sampling_state = SAMPLING_RESAMPLE_SOURCE;
            dev->resample_source_pad = -1;
            dev->resample_dest_pad = -1;
            display_raw(dev, SEG_LEV[0], SEG_LEV[1], SEG_LEV[2]);
        }
        return;
    }

    if (btn == BTN_START_END_LEVEL) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            !pattern_mode_active(dev) &&
            dev->last_played_pad >= 0) {
            dev->sampling_state = SAMPLING_EDIT;
            dev->edit_display_locked = false;
        }
        return;
    }

    if (btn == BTN_LOOP) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad]) {
            dev->pad_loop_mode[dev->last_played_pad] = !dev->pad_loop_mode[dev->last_played_pad];
        }
        return;
    }

    if (btn == BTN_GATE) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad]) {
            dev->pad_gate_mode[dev->last_played_pad] = !dev->pad_gate_mode[dev->last_played_pad];
        }
        return;
    }

    if (btn == BTN_REVERSE) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad]) {
            dev->pad_reverse_mode[dev->last_played_pad] = !dev->pad_reverse_mode[dev->last_played_pad];
        }
        return;
    }

    if (btn == BTN_STEREO) {
        bool can_change =
            dev->sampling_state == SAMPLING_STANDBY ||
            dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
            dev->sampling_state == SAMPLING_RESAMPLE_DEST;
        if (!can_change) {
            return;
        }
        dev->sampling_stereo = !dev->sampling_stereo;
        return;
    }

    if (btn == BTN_LONG_LOFI) {
        bool can_change =
            dev->sampling_state == SAMPLING_STANDBY ||
            dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
            dev->sampling_state == SAMPLING_RESAMPLE_DEST;
        if (!can_change) {
            return;
        }
        if (dev->sampling_quality == SAMPLE_QUALITY_STANDARD) {
            dev->sampling_quality = SAMPLE_QUALITY_LONG;
        } else if (dev->sampling_quality == SAMPLE_QUALITY_LONG) {
            dev->sampling_quality = SAMPLE_QUALITY_LOFI;
        } else {
            dev->sampling_quality = SAMPLE_QUALITY_STANDARD;
        }
        return;
    }

    // ─── CANCEL button ────────────────────────────────────────────────────────
    if (btn == BTN_CANCEL) {
        if (dev->sampling_state != SAMPLING_IDLE && dev->sampling_state != SAMPLING_THRESHOLD) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->sampling_target_pad = -1;
            dev->sampling_full = false;
            dev->bpm_edit_mode = false;
            dev->tap_count = 0;
            dev->delete_target_pad = -1;
            dev->delete_all_group = -1;
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        }
        return;
    }

    // ─── Effect button routing ────────────────────────────────────────────────
    if ((dev->sampling_state == SAMPLING_IDLE ||
         dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
         dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
         dev->sampling_state == SAMPLING_RESAMPLE_ARMED) &&
        is_effect_btn(btn)) {
        if (btn == BTN_MFX) {
            dev->mfx_knob_touched_while_pressed = false;
        } else {
            apply_effect_button_press(dev, btn);
        }
        return;
    }

    // ─── Pad press during REMAIN hold (toggle effect) ─────────────────────────
    if (dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_REMAIN].pressed &&
        button_to_actual_pad(dev, btn) >= 0) {
        int actual_pad = button_to_actual_pad(dev, btn);
        if (dev->active_effect_btn >= 0) {
            dev->pad_has_effect[actual_pad] = !dev->pad_has_effect[actual_pad];
        }
        return;
    }

    // ─── Pad selection during sampling standby ────────────────────────────────
    if (dev->sampling_state == SAMPLING_STANDBY) {
        int actual_pad = button_to_actual_pad(dev, btn);
        if (actual_pad >= 0 && actual_pad < 32) {
            dev->sampling_target_pad = actual_pad;
            return;
        }
    }

    if (dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_MARK].pressed &&
        button_to_actual_pad(dev, btn) >= 0) {
        int actual_pad = button_to_actual_pad(dev, btn);
        std::printf("[MARK] held + pad press detected -> bank %c pad %d has_sample=%d is_playing=%d last_played=%d\n",
                    pad_bank_name(actual_pad), pad_bank_number(actual_pad),
                    dev->pad_has_sample[actual_pad] ? 1 : 0,
                    dev->pad_is_playing[actual_pad] ? 1 : 0,
                    dev->last_played_pad);
        if (dev->pad_has_sample[actual_pad]) {
            std::printf("[MARK] held while pad pressed -> bank %c pad %d, enter END_ONLY\n",
                        pad_bank_name(actual_pad), pad_bank_number(actual_pad));
            dev->mark_pending_action = 0;
            dev->mark_action_pad = -1;
            dev->mark_edit_pad = actual_pad;
            dev->last_played_pad = actual_pad;
            dev->mark_end_only_lock_pad = actual_pad;
            dev->sampling_state = SAMPLING_MARK_END_ONLY;
            return;
        }
    }
}

void button_up(Device* dev, ButtonID btn) {
    if (!dev || btn < 0 || btn >= BTN_COUNT) return;
    dev->state.buttons[btn].pressed = false;

    if ((dev->sampling_state == SAMPLING_IDLE ||
         dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
         dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
         dev->sampling_state == SAMPLING_RESAMPLE_ARMED) &&
        btn == BTN_MFX) {
        if (!dev->mfx_knob_touched_while_pressed) {
            apply_effect_button_press(dev, BTN_MFX);
        }
        dev->mfx_knob_touched_while_pressed = false;
    }

    if (dev->pattern_erase_mode &&
        btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
        int sample_slot = btn - BTN_PAD_1;
        pattern_set_erase_pad(&dev->pattern_seq, sample_slot, false);
    }

    if (dev->sampling_state == SAMPLING_MARK_EDIT &&
        dev->mark_edit_pad >= 0 &&
        button_to_actual_pad(dev, btn) == dev->mark_edit_pad) {
        dev->sampling_state = SAMPLING_IDLE;
        dev->mark_edit_pad = -1;
    }
}

void knob_set(Device* dev, KnobID knob, float value) {
    if (!dev || knob < 0 || knob >= KNOB_COUNT) return;
    float clamped = std::clamp(value, 0.0f, 1.0f);
    dev->state.knobs[knob].value = clamped;

    if (knob == KNOB_RESONANCE &&
        dev->sampling_state == SAMPLING_IDLE &&
        dev->pattern_mode &&
        dev->pattern_bpm_edit_mode &&
        !dev->pattern_recording) {
        int bpm = 40 + (int)std::lround(clamped * 160.0f);
        pattern_set_bpm(&dev->pattern_seq, bpm);
    }
    if (knob == KNOB_DRIVE &&
        dev->sampling_state == SAMPLING_IDLE) {
        int pattern_edit_slot = dev->pattern_record_slot;
        if (pattern_edit_slot < 0 && pattern_is_playing(&dev->pattern_seq)) {
            pattern_edit_slot = pattern_get_current_slot(&dev->pattern_seq);
        }
        if (pattern_edit_slot < 0) {
            for (int i = 0; i < 32; ++i) {
                if (pattern_has_data(&dev->pattern_seq, i)) {
                    pattern_edit_slot = i;
                    break;
                }
            }
        }
        if (pattern_edit_slot >= 0) {
            if (dev->pattern_metronome_edit_mode) {
                pattern_set_metronome_level(&dev->pattern_seq, pattern_edit_slot,
                    std::clamp((int)std::lround(clamped * 127.0f), 0, 127));
            } else if (dev->pattern_length_edit_mode) {
                int measures = (clamped <= 0.0f) ? 1 : std::clamp((int)std::lround(1.0f + clamped * 98.0f), 1, 99);
                if (measures > 20) {
                    int snapped = 20 + (int)std::lround((measures - 20) / 4.0f) * 4;
                    measures = std::clamp(snapped, 20, 99);
                }
                pattern_set_length_measures(&dev->pattern_seq, pattern_edit_slot, measures);
            } else if (dev->pattern_quantize_edit_mode) {
                int idx = std::clamp((int)std::lround(clamped * 4.0f), 0, 4);
                pattern_set_quantize(&dev->pattern_seq, pattern_edit_slot, (PatternQuantize)idx);
            }
        }
    }

    if (knob == KNOB_RESONANCE &&
        dev->sampling_state == SAMPLING_STANDBY &&
        dev->bpm_edit_mode) {
        if (clamped <= 0.01f) {
            dev->bpm_value = -1;
            dev->bpm_armed_for_next_sample = false;
        } else {
            int bpm = 40 + (int)std::lround(clamped * 160.0f);
            dev->bpm_value = std::clamp(bpm, 40, 200);
        }
    }

    if (knob == KNOB_DRIVE && dev->sampling_state == SAMPLING_THRESHOLD) {
        dev->sample_level_threshold = std::clamp((int)std::lround(clamped * 8.0f), 0, 8);
    }
    if (knob == KNOB_DRIVE &&
        (dev->sampling_state == SAMPLING_RECORDING ||
         dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
         dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
         dev->sampling_state == SAMPLING_RESAMPLE_ARMED ||
         dev->sampling_state == SAMPLING_RESAMPLE_RECORDING)) {
        dev->rec_gain_value = std::clamp((int)std::lround(clamped * 127.0f), 0, 127);
        dev->rec_gain_display_frames = 30;
    }

    // MFX sub-type cycling: hold MFX + turn CTRL 3 → select 1 of 21 effects
    if (knob == KNOB_DRIVE &&
        dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_MFX].pressed) {
        dev->mfx_knob_touched_while_pressed = true;
        dev->mfx_type = std::clamp((int)std::lround(clamped * 20.0f), 0, 20);
        set_display_number_plain(dev, dev->mfx_type + 1);
    }
}

void indicator_set(Device* dev, IndicatorID ind, bool lit) {
    if (!dev || ind < 0 || ind >= IND_COUNT) return;
    dev->state.indicators[ind].lit = lit;
}

// ─── Clock ────────────────────────────────────────────────────────────────────

void tick(Device* dev, uint32_t samples_elapsed) {
    if (!dev) return;
    dev->sample_clock += samples_elapsed;

    dev->blink_phase = (dev->blink_phase + 1) % 60;
    dev->blink_on = (dev->blink_phase < 30);
    pattern_advance(&dev->pattern_seq, samples_elapsed, CORE_TAP_SAMPLE_RATE);

    for (int i = 0; i < 32; ++i) {
        if (dev->pad_led_hold_frames[i] > 0) dev->pad_led_hold_frames[i]--;
    }
    if (dev->rec_gain_display_frames > 0 &&
        dev->sampling_state != SAMPLING_RESAMPLE_SOURCE &&
        dev->sampling_state != SAMPLING_RESAMPLE_DEST &&
        dev->sampling_state != SAMPLING_RESAMPLE_ARMED) {
        dev->rec_gain_display_frames--;
    }
    if (dev->pattern_tap_display_frames > 0) {
        dev->pattern_tap_display_frames--;
    }
    if (dev->transient_display_frames > 0) {
        dev->transient_display_frames--;
        if (dev->transient_display_frames == 0) {
            if (dev->card_emp_lock) {
                std::printf("[CARD] EMP lock timeout reached\n");
                finish_card_emp_lock(dev);
                return;
            }
            if (dev->sampling_state == SAMPLING_IDLE) {
                if (pattern_mode_active(dev)) display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
                else display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            }
        }
    }

    // ─── Update LED states ────────────────────────────────────────────────────

    dev->state.buttons[BTN_REC].lit = false;
    dev->state.buttons[BTN_START_END_LEVEL].lit = false;
    dev->state.buttons[BTN_TIME_BPM].lit = false;
    dev->state.buttons[BTN_PATTERN_SELECT].lit = false;
    dev->state.buttons[BTN_DEL].lit = false;
    dev->state.buttons[BTN_RESAMPLE].lit = false;
    dev->state.buttons[BTN_GATE].lit = false;
    dev->state.buttons[BTN_LOOP].lit = false;
    dev->state.buttons[BTN_STEREO].lit = false;
    dev->state.buttons[BTN_LONG_LOFI].lit = false;
    dev->state.buttons[BTN_FILTER_DRIVE].lit = false;
    dev->state.buttons[BTN_PITCH].lit = false;
    dev->state.buttons[BTN_DELAY].lit = false;
    dev->state.buttons[BTN_MFX].lit = false;
    dev->state.buttons[BTN_VINYL_SIM].lit = false;
    dev->state.buttons[BTN_ISOLATOR].lit = false;
    for (int i = 0; i < 32; ++i) {
        dev->state.buttons[BTN_PAD_1 + i].lit = false;
    }

    if (dev->card_emp_lock) {
        std::printf("[CARD] render EMP lock display bank=%c frames=%d\n",
                    "ABCD"[dev->card_emp_return_bank % 4],
                    dev->transient_display_frames);
        if (dev->card_emp_group == 0) {
            dev->state.buttons[BTN_BANK_A].lit = true;
            dev->state.buttons[BTN_BANK_B].lit = true;
        } else {
            dev->state.buttons[BTN_BANK_C].lit = true;
            dev->state.buttons[BTN_BANK_D].lit = true;
        }
        display_raw(dev, SEG_EMP[0], SEG_EMP[1], SEG_EMP[2]);
    }
    else if (dev->sampling_state == SAMPLING_IDLE && pattern_mode_active(dev)) {
        bool pattern_ui = pattern_ui_active(dev);
        bool pattern_playing = pattern_is_playing(&dev->pattern_seq);
        dev->state.buttons[BTN_PATTERN_SELECT].lit = pattern_ui;
        dev->state.buttons[BTN_TIME_BPM].lit = dev->pattern_bpm_edit_mode;
        dev->state.buttons[BTN_START_END_LEVEL].lit = dev->pattern_metronome_edit_mode;
        dev->state.buttons[BTN_LENGTH].lit = dev->pattern_length_edit_mode;
        dev->state.buttons[BTN_QUANTIZE].lit = dev->pattern_quantize_edit_mode;

        int bank_start = dev->state.active_bank * 8;
        int playing_slot = pattern_get_current_slot(&dev->pattern_seq);
        int record_slot = dev->pattern_record_slot;

        if (dev->pattern_recording) {
            dev->state.buttons[BTN_REC].lit = true;
            dev->state.buttons[BTN_DEL].lit = dev->pattern_erase_mode;
            if (pattern_is_count_in(&dev->pattern_seq) &&
                record_slot >= bank_start && record_slot < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, record_slot - bank_start)].lit = true;
            } else if (!dev->pattern_erase_mode) {
                render_visible_sample_trigger_leds(dev, bank_start);
            }
            if (dev->pattern_erase_mode) {
                display_raw(dev, SEG_ERS[0], SEG_ERS[1], SEG_ERS[2]);
            } else if (dev->pattern_quantize_edit_mode) {
                PatternQuantize q = pattern_get_quantize(&dev->pattern_seq, record_slot);
                if (q == PATTERN_QUANTIZE_OFF) {
                    display_raw(dev, SEG_OFF[0], SEG_OFF[1], SEG_OFF[2]);
                } else if (q == PATTERN_QUANTIZE_4) {
                    display_raw(dev, SEG_BLANK, SEG_BLANK, SEG_DIGITS[4]);
                } else if (q == PATTERN_QUANTIZE_8) {
                    display_raw(dev, SEG_BLANK, SEG_BLANK, SEG_DIGITS[8]);
                } else if (q == PATTERN_QUANTIZE_8T) {
                    display_raw(dev, SEG_DIGITS[8], SEG_DASH, SEG_DIGITS[3]);
                } else {
                    display_raw(dev, SEG_BLANK, SEG_DIGITS[1], SEG_DIGITS[6]);
                }
            } else if (pattern_is_count_in(&dev->pattern_seq)) {
                display_raw(dev, SEG_DASH, SEG_DIGITS[pattern_get_count_in_beat(&dev->pattern_seq)], SEG_BLANK);
            } else {
                set_display_measure_beat(dev,
                                         pattern_get_current_measure(&dev->pattern_seq),
                                         pattern_get_current_beat(&dev->pattern_seq));
            }
        } else if (dev->pattern_delete_all_select || dev->pattern_delete_all_armed) {
            dev->state.buttons[BTN_DEL].lit = !dev->pattern_delete_all_armed || dev->blink_on;
            if (dev->pattern_delete_all_armed) {
                if (dev->pattern_delete_all_group == 0) {
                    dev->state.buttons[BTN_BANK_A].lit = true;
                    dev->state.buttons[BTN_BANK_B].lit = true;
                } else if (dev->pattern_delete_all_group == 1) {
                    dev->state.buttons[BTN_BANK_C].lit = true;
                    dev->state.buttons[BTN_BANK_D].lit = true;
                }
            } else {
                dev->state.buttons[BTN_BANK_A].lit = dev->blink_on;
                dev->state.buttons[BTN_BANK_B].lit = dev->blink_on;
                dev->state.buttons[BTN_BANK_C].lit = dev->blink_on;
                dev->state.buttons[BTN_BANK_D].lit = dev->blink_on;
            }
            display_raw(dev, SEG_DAL[0], SEG_DAL[1], SEG_DAL[2]);
        } else if (dev->pattern_swap_select_a || dev->pattern_swap_select_b) {
            dev->state.buttons[BTN_DEL].lit = true;
            dev->state.buttons[BTN_REC].lit = dev->pattern_swap_select_b ? dev->blink_on : true;
            for (int i = 0; i < 8; ++i) {
                int slot = bank_start + i;
                if (pattern_has_data(&dev->pattern_seq, slot)) {
                    dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                }
            }
            if (dev->pattern_swap_a >= bank_start && dev->pattern_swap_a < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, dev->pattern_swap_a - bank_start)].lit = true;
            }
            if (dev->pattern_swap_b >= bank_start && dev->pattern_swap_b < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, dev->pattern_swap_b - bank_start)].lit = true;
            }
            display_raw(dev, SEG_CHG[0], SEG_CHG[1], SEG_CHG[2]);
        } else if (dev->pattern_delete_select) {
            dev->state.buttons[BTN_DEL].lit = (dev->pattern_delete_slot >= 0) ? dev->blink_on : true;
            for (int i = 0; i < 8; ++i) {
                int slot = bank_start + i;
                if (pattern_has_data(&dev->pattern_seq, slot)) {
                    dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                }
            }
            if (dev->pattern_delete_slot >= bank_start && dev->pattern_delete_slot < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, dev->pattern_delete_slot - bank_start)].lit = true;
            }
            display_raw(dev, SEG_DEL[0], SEG_DEL[1], SEG_DEL[2]);
        } else if (dev->pattern_record_select) {
            dev->state.buttons[BTN_REC].lit = (record_slot >= 0) ? dev->blink_on : true;
            if (record_slot < 0) {
                for (int i = 0; i < 8; ++i) {
                    int slot = bank_start + i;
                    if (!pattern_has_data(&dev->pattern_seq, slot)) {
                        dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                    }
                }
            }
            if (record_slot >= bank_start && record_slot < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, record_slot - bank_start)].lit = true;
            }
            if (record_slot >= 0) {
                if (dev->pattern_metronome_edit_mode) {
                    set_display_number_plain(dev, pattern_get_metronome_level(&dev->pattern_seq, record_slot));
                } else if (dev->pattern_bpm_edit_mode) {
                    set_display_number_plain(dev, pattern_get_bpm(&dev->pattern_seq));
                } else if (dev->pattern_length_edit_mode) {
                    set_display_number_plain(dev, pattern_get_length_measures(&dev->pattern_seq, record_slot));
                } else if (dev->pattern_quantize_edit_mode) {
                    PatternQuantize q = pattern_get_quantize(&dev->pattern_seq, record_slot);
                    if (q == PATTERN_QUANTIZE_OFF) {
                        display_raw(dev, SEG_OFF[0], SEG_OFF[1], SEG_OFF[2]);
                    } else if (q == PATTERN_QUANTIZE_4) {
                        display_raw(dev, SEG_BLANK, SEG_BLANK, SEG_DIGITS[4]);
                    } else if (q == PATTERN_QUANTIZE_8) {
                        display_raw(dev, SEG_BLANK, SEG_BLANK, SEG_DIGITS[8]);
                    } else if (q == PATTERN_QUANTIZE_8T) {
                        display_raw(dev, SEG_DIGITS[8], SEG_DASH, SEG_DIGITS[3]);
                    } else {
                        display_raw(dev, SEG_BLANK, SEG_DIGITS[1], SEG_DIGITS[6]);
                    }
                } else {
                    display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
                }
            } else {
                display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
            }
        } else if (!pattern_ui && pattern_playing) {
            render_visible_sample_trigger_leds(dev, bank_start);
            if (dev->pattern_tap_display_frames > 0) {
                set_display_number_plain(dev, dev->pattern_tap_display_value);
            } else if (dev->pattern_bpm_edit_mode) {
                set_display_number_plain(dev, pattern_get_bpm(&dev->pattern_seq));
            } else if (dev->pattern_metronome_edit_mode) {
                int target_slot = (record_slot >= 0) ? record_slot : playing_slot;
                if (target_slot >= 0) {
                    set_display_number_plain(dev, pattern_get_metronome_level(&dev->pattern_seq, target_slot));
                } else {
                    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                }
            } else {
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                int slot = bank_start + i;
                if (pattern_has_data(&dev->pattern_seq, slot)) {
                    dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                }
            }
            if (playing_slot >= bank_start && playing_slot < bank_start + 8) {
                dev->state.buttons[visible_pad_button_id(dev, playing_slot - bank_start)].lit = true;
            }
            if (dev->pattern_tap_display_frames > 0) {
                set_display_number_plain(dev, dev->pattern_tap_display_value);
            } else if (dev->pattern_bpm_edit_mode) {
                set_display_number_plain(dev, pattern_get_bpm(&dev->pattern_seq));
            } else if (dev->pattern_metronome_edit_mode) {
                int target_slot = (record_slot >= 0) ? record_slot : playing_slot;
                if (target_slot >= 0) {
                    set_display_number_plain(dev, pattern_get_metronome_level(&dev->pattern_seq, target_slot));
                } else {
                    display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
                }
            } else {
                display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
            }
        }
    }
    else if (dev->sampling_state == SAMPLING_IDLE) {
        dev->state.buttons[BTN_TIME_BPM].lit = dev->time_bpm_mode;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i] && dev->pad_is_playing[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = true;
            }
        }
    }
    else if (dev->sampling_state == SAMPLING_THRESHOLD) {
        dev->state.buttons[BTN_REC].lit = dev->blink_on;
        display_raw(dev,
                    SEG_DASH,
                    SEG_DIGITS[dev->sample_level_threshold],
                    SEG_DASH);
    }
    else if (dev->sampling_state == SAMPLING_STANDBY) {
        dev->state.buttons[BTN_REC].lit = dev->blink_on;
        dev->state.buttons[BTN_TIME_BPM].lit = dev->bpm_edit_mode;

        int bank_start = dev->state.active_bank * 8;

        if (dev->sampling_target_pad >= 0) {
            int selected_local = dev->sampling_target_pad - bank_start;
            for (int i = 0; i < 8; ++i) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = (i == selected_local);
            }
            if (dev->bpm_edit_mode) {
                if (dev->bpm_value >= 40 && dev->bpm_value <= 200) {
                    set_display_number_plain(dev, dev->bpm_value);
                } else {
                    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                }
            } else {
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                if (!dev->pad_has_sample[bank_start + i]) {
                    dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                }
            }
            if (dev->bpm_edit_mode) {
                if (dev->bpm_value >= 40 && dev->bpm_value <= 200) {
                    set_display_number_plain(dev, dev->bpm_value);
                } else {
                    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
                }
            } else {
                display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            }
        }
    }
    else if (dev->sampling_state == SAMPLING_READY) {
        dev->state.buttons[BTN_REC].lit = true;
        if (dev->sampling_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->sampling_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[visible_pad_button_id(dev, local_pad)].lit = true;
            }
        }
        display_raw(dev, SEG_RDY[0], SEG_RDY[1], SEG_RDY[2]);
    }
    else if (dev->sampling_state == SAMPLING_RECORDING) {
        dev->state.buttons[BTN_REC].lit = dev->blink_on;

        if (dev->sampling_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->sampling_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[visible_pad_button_id(dev, local_pad)].lit = true;
            }
        }

        if (dev->sampling_full) {
            display_raw(dev, SEG_FUL[0], SEG_FUL[1], SEG_FUL[2]);
        } else if (dev->rec_gain_display_frames > 0) {
            set_display_number_plain(dev, dev->rec_gain_value);
        } else {
            display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_RESAMPLE_SOURCE) {
        dev->state.buttons[BTN_RESAMPLE].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i] && dev->pad_is_playing[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = true;
            }
        }
        if (dev->rec_gain_display_frames > 0) {
            set_display_number_plain(dev, dev->rec_gain_value);
        } else {
            display_raw(dev, SEG_LEV[0], SEG_LEV[1], SEG_LEV[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_RESAMPLE_DEST) {
        dev->state.buttons[BTN_RESAMPLE].lit = true;
        dev->state.buttons[BTN_REC].lit = (dev->resample_dest_pad >= 0) ? dev->blink_on : true;
        int bank_start = dev->state.active_bank * 8;
        if (dev->resample_dest_pad >= 0) {
            int local_pad = dev->resample_dest_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[visible_pad_button_id(dev, local_pad)].lit = true;
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                int pad = bank_start + i;
                if (!dev->pad_has_sample[pad]) {
                    dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
                }
            }
        }
        if (dev->rec_gain_display_frames > 0) {
            set_display_number_plain(dev, dev->rec_gain_value);
        } else {
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        }
    }
    else if (dev->sampling_state == SAMPLING_RESAMPLE_ARMED) {
        dev->state.buttons[BTN_RESAMPLE].lit = true;
        dev->state.buttons[BTN_REC].lit = true;
        int bank_start = dev->state.active_bank * 8;
        int local_dst = dev->resample_dest_pad - bank_start;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
            }
        }
        if (local_dst >= 0 && local_dst < 8) dev->state.buttons[visible_pad_button_id(dev, local_dst)].lit = true;
        if (dev->rec_gain_display_frames > 0) {
            set_display_number_plain(dev, dev->rec_gain_value);
        } else {
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        }
    }
    else if (dev->sampling_state == SAMPLING_RESAMPLE_RECORDING) {
        dev->state.buttons[BTN_RESAMPLE].lit = true;
        dev->state.buttons[BTN_REC].lit = true;
        int bank_start = dev->state.active_bank * 8;
        int local_dst = dev->resample_dest_pad - bank_start;
        if (local_dst >= 0 && local_dst < 8) {
            dev->state.buttons[visible_pad_button_id(dev, local_dst)].lit = true;
        }
        if (dev->sampling_full) {
            display_raw(dev, SEG_FUL[0], SEG_FUL[1], SEG_FUL[2]);
        } else {
            display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_EDIT) {
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i] && dev->pad_is_playing[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = true;
            }
        }
        dev->state.buttons[BTN_START_END_LEVEL].lit = true;
        if (!dev->edit_display_locked) {
            display_raw(dev, SEG_EDT[0], SEG_EDT[1], SEG_EDT[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_SWAP_SELECT_A) {
        dev->state.buttons[BTN_DEL].lit = true;
        dev->state.buttons[BTN_REC].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
            }
        }
        display_raw(dev, SEG_CHG[0], SEG_CHG[1], SEG_CHG[2]);
    }
    else if (dev->sampling_state == SAMPLING_SWAP_SELECT_B) {
        dev->state.buttons[BTN_DEL].lit = true;
        int bank_start = dev->state.active_bank * 8;
        int local_a = (dev->swap_pad_a >= 0) ? (dev->swap_pad_a - bank_start) : -1;
        int local_b = (dev->swap_pad_b >= 0) ? (dev->swap_pad_b - bank_start) : -1;

        if (dev->swap_pad_b >= 0 && dev->swap_pad_b != dev->swap_pad_a) {
            dev->state.buttons[BTN_REC].lit = dev->blink_on;
            if (local_a >= 0 && local_a < 8) dev->state.buttons[visible_pad_button_id(dev, local_a)].lit = true;
            if (local_b >= 0 && local_b < 8) dev->state.buttons[visible_pad_button_id(dev, local_b)].lit = true;
        } else {
            dev->state.buttons[BTN_REC].lit = true;
            if (local_a >= 0 && local_a < 8) dev->state.buttons[visible_pad_button_id(dev, local_a)].lit = true;
        }
        display_raw(dev, SEG_CHG[0], SEG_CHG[1], SEG_CHG[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        dev->state.buttons[BTN_DEL].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
            }
        }
        display_raw(dev, SEG_DEL[0], SEG_DEL[1], SEG_DEL[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ARMED) {
        dev->state.buttons[BTN_DEL].lit = dev->blink_on;
        if (dev->delete_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->delete_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[visible_pad_button_id(dev, local_pad)].lit = true;
            }
        }
        display_raw(dev, SEG_DEL[0], SEG_DEL[1], SEG_DEL[2]);
    }
    else if (dev->sampling_state == SAMPLING_TRUNCATE_ARMED) {
        dev->state.buttons[BTN_DEL].lit = dev->blink_on;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i] &&
                dev->pad_is_marked[bank_start + i]) {
                dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->blink_on;
            }
        }
        if (dev->last_played_pad >= 0 &&
            dev->pad_is_marked[dev->last_played_pad]) {
            int local_pad = dev->last_played_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[visible_pad_button_id(dev, local_pad)].lit = true;
            }
        }
        display_raw(dev, SEG_TRC[0], SEG_TRC[1], SEG_TRC[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ALL_SELECT) {
        dev->state.buttons[BTN_DEL].lit = true;
        dev->state.buttons[BTN_BANK_A].lit = dev->blink_on;
        dev->state.buttons[BTN_BANK_B].lit = dev->blink_on;
        dev->state.buttons[BTN_BANK_C].lit = dev->blink_on;
        dev->state.buttons[BTN_BANK_D].lit = dev->blink_on;
        display_raw(dev, SEG_DAL[0], SEG_DAL[1], SEG_DAL[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ALL_ARMED) {
        dev->state.buttons[BTN_DEL].lit = dev->blink_on;
        if (dev->delete_all_group == 0) {
            dev->state.buttons[BTN_BANK_A].lit = true;
            dev->state.buttons[BTN_BANK_B].lit = true;
        } else if (dev->delete_all_group == 1) {
            dev->state.buttons[BTN_BANK_C].lit = true;
            dev->state.buttons[BTN_BANK_D].lit = true;
        }
        display_raw(dev, SEG_DAL[0], SEG_DAL[1], SEG_DAL[2]);
    }
    else if (dev->sampling_state == SAMPLING_CARD_FORMAT_SELECT) {
        dev->state.buttons[BTN_BANK_C].lit = dev->blink_on;
        dev->state.buttons[BTN_BANK_D].lit = dev->blink_on;
        display_raw(dev, SEG_FMT[0], SEG_FMT[1], SEG_FMT[2]);
    }
    else if (dev->sampling_state == SAMPLING_CARD_FORMAT_ARMED) {
        dev->state.buttons[BTN_DEL].lit = dev->blink_on;
        if (dev->memory_card_selected_bank == BTN_BANK_C ||
            dev->memory_card_selected_bank == BTN_BANK_D) {
            dev->state.buttons[BTN_BANK_C].lit = true;
            dev->state.buttons[BTN_BANK_D].lit = true;
        }
        display_raw(dev, SEG_FMT[0], SEG_FMT[1], SEG_FMT[2]);
    }
    else if (dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_SELECT ||
             dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_ARMED) {
        dev->state.buttons[BTN_BANK_C].lit = true;
        dev->state.buttons[BTN_BANK_D].lit = true;
        if (dev->sampling_state == SAMPLING_CARD_SAMPLE_SAVE_ARMED) {
            dev->state.buttons[BTN_REC].lit = dev->blink_on;
        }
        for (int i = 0; i < 8; ++i) {
            bool occupied = get_memory_card_backup_kind(dev, i) == MEMORY_CARD_BACKUP_SAMPLES;
            dev->state.buttons[visible_pad_button_id(dev, i)].lit = occupied ? true : dev->blink_on;
        }
        if (dev->memory_card_backup_slot >= 0) {
            dev->state.buttons[visible_pad_button_id(dev, dev->memory_card_backup_slot)].lit = true;
        }
        display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
    }
    else if (dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_SELECT ||
             dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_ARMED) {
        dev->state.buttons[BTN_BANK_A].lit = true;
        dev->state.buttons[BTN_BANK_B].lit = true;
        if (dev->sampling_state == SAMPLING_CARD_SAMPLE_LOAD_ARMED) {
            dev->state.buttons[BTN_REC].lit = dev->blink_on;
        }
        bool any = false;
        for (int i = 0; i < 8; ++i) {
            bool has = get_memory_card_backup_kind(dev, i) == MEMORY_CARD_BACKUP_SAMPLES;
            any = any || has;
            dev->state.buttons[visible_pad_button_id(dev, i)].lit = has && (dev->memory_card_backup_slot == i || dev->blink_on);
        }
        if (dev->memory_card_backup_slot >= 0) {
            dev->state.buttons[visible_pad_button_id(dev, dev->memory_card_backup_slot)].lit = true;
        }
        if (any) {
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
        } else {
            display_raw(dev, SEG_EMP[0], SEG_EMP[1], SEG_EMP[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_SELECT ||
             dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_ARMED) {
        dev->state.buttons[BTN_PATTERN_SELECT].lit = true;
        dev->state.buttons[BTN_BANK_C].lit = true;
        dev->state.buttons[BTN_BANK_D].lit = true;
        if (dev->sampling_state == SAMPLING_CARD_PATTERN_SAVE_ARMED) {
            dev->state.buttons[BTN_REC].lit = dev->blink_on;
        }
        for (int i = 0; i < 8; ++i) {
            bool occupied = get_memory_card_backup_kind(dev, i) == MEMORY_CARD_BACKUP_PATTERNS;
            dev->state.buttons[visible_pad_button_id(dev, i)].lit = occupied ? true : dev->blink_on;
        }
        if (dev->memory_card_backup_slot >= 0) {
            dev->state.buttons[visible_pad_button_id(dev, dev->memory_card_backup_slot)].lit = true;
        }
        display_raw(dev, SEG_REC[0], SEG_REC[1], SEG_REC[2]);
    }
    else if (dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_SELECT ||
             dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_ARMED) {
        dev->state.buttons[BTN_PATTERN_SELECT].lit = true;
        dev->state.buttons[BTN_BANK_A].lit = true;
        dev->state.buttons[BTN_BANK_B].lit = true;
        if (dev->sampling_state == SAMPLING_CARD_PATTERN_LOAD_ARMED) {
            dev->state.buttons[BTN_REC].lit = dev->blink_on;
        }
        bool any = false;
        for (int i = 0; i < 8; ++i) {
            bool has = get_memory_card_backup_kind(dev, i) == MEMORY_CARD_BACKUP_PATTERNS;
            any = any || has;
            dev->state.buttons[visible_pad_button_id(dev, i)].lit = has && (dev->memory_card_backup_slot == i || dev->blink_on);
        }
        if (dev->memory_card_backup_slot >= 0) {
            dev->state.buttons[visible_pad_button_id(dev, dev->memory_card_backup_slot)].lit = true;
        }
        if (any) {
            display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
        } else {
            display_raw(dev, SEG_EMP[0], SEG_EMP[1], SEG_EMP[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_MARK_EDIT ||
             dev->sampling_state == SAMPLING_MARK_END_ONLY) {
        int bank_start = dev->state.active_bank * 8;
        render_visible_sample_trigger_leds(dev, bank_start);
        dev->state.buttons[BTN_MARK].lit = dev->blink_on;
        display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
    }

    if (!dev->card_emp_lock &&
        !pattern_mode_active(dev) &&
        !is_delete_mode(dev) &&
        dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
        dev->pad_has_sample[dev->last_played_pad]) {
        dev->state.buttons[BTN_LOOP].lit = dev->pad_loop_mode[dev->last_played_pad];
        dev->state.buttons[BTN_GATE].lit = dev->pad_gate_mode[dev->last_played_pad];
        dev->state.buttons[BTN_REVERSE].lit = dev->pad_reverse_mode[dev->last_played_pad];
    }

    bool sampling_mode_lights =
        !dev->card_emp_lock &&
        (dev->sampling_state == SAMPLING_STANDBY ||
         dev->sampling_state == SAMPLING_READY ||
         dev->sampling_state == SAMPLING_RECORDING ||
         dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
         dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
         dev->sampling_state == SAMPLING_RESAMPLE_ARMED ||
         dev->sampling_state == SAMPLING_RESAMPLE_RECORDING);
    if (sampling_mode_lights && dev->sampling_stereo) {
        dev->state.buttons[BTN_STEREO].lit = true;
    }
    if (sampling_mode_lights && dev->sampling_quality == SAMPLE_QUALITY_LONG) {
        dev->state.buttons[BTN_LONG_LOFI].lit = true;
    } else if (sampling_mode_lights && dev->sampling_quality == SAMPLE_QUALITY_LOFI) {
        dev->state.buttons[BTN_LONG_LOFI].lit = dev->blink_on;
    } else if (!dev->card_emp_lock &&
               !pattern_mode_active(dev) &&
               dev->last_played_pad >= 0 &&
               dev->last_played_pad < 32 &&
               dev->pad_has_sample[dev->last_played_pad]) {
        if (dev->pad_recorded_stereo[dev->last_played_pad]) {
            dev->state.buttons[BTN_STEREO].lit = true;
        }
        if (dev->pad_recorded_quality[dev->last_played_pad] == SAMPLE_QUALITY_LONG) {
            dev->state.buttons[BTN_LONG_LOFI].lit = true;
        } else if (dev->pad_recorded_quality[dev->last_played_pad] == SAMPLE_QUALITY_LOFI) {
            dev->state.buttons[BTN_LONG_LOFI].lit = dev->blink_on;
        }
    }

    // ─── Effect button LED — lit when selected pad carries the active effect ───
    if (!dev->card_emp_lock &&
        dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
        dev->pad_has_effect[dev->last_played_pad] && dev->active_effect_btn >= 0) {
        dev->state.buttons[dev->active_effect_btn].lit = true;
    }

    // ─── REMAIN hold — pad LEDs show which pads carry the active effect ───────
    if (!dev->card_emp_lock &&
        dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_REMAIN].pressed &&
        dev->active_effect_btn >= 0) {
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            dev->state.buttons[visible_pad_button_id(dev, i)].lit = dev->pad_has_effect[bank_start + i];
        }
    }

    // ─── MFX held — show sub-type number (1-21) on display ───────────────────
    if (!dev->card_emp_lock &&
        dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_MFX].pressed &&
        dev->mfx_knob_touched_while_pressed) {
        dev->transient_display_frames = 0;
        set_display_number_plain(dev, dev->mfx_type + 1);
    }

    if (dev->transient_display_frames > 0) {
        display_raw(dev,
                    dev->transient_display[0],
                    dev->transient_display[1],
                    dev->transient_display[2]);
    }
}

// ─── Output ───────────────────────────────────────────────────────────────────

State get_state(const Device* dev) {
    State out;
    std::memset(&out, 0, sizeof(out));
    if (dev) out = dev->state;
    return out;
}

// ─── Sampling control ─────────────────────────────────────────────────────────

bool is_sampling_standby(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_STANDBY;
}

bool is_sampling_ready(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_READY;
}

bool is_recording(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_RECORDING ||
           dev->sampling_state == SAMPLING_RESAMPLE_RECORDING;
}

bool is_pattern_mode(const Device* dev) {
    return pattern_mode_active(dev);
}

bool is_pattern_recording(const Device* dev) {
    if (!dev) return false;
    return dev->pattern_recording;
}

bool is_pattern_record_select(const Device* dev) {
    if (!dev) return false;
    return dev->pattern_record_select;
}

bool is_pattern_erase_mode(const Device* dev) {
    if (!dev) return false;
    return dev->pattern_erase_mode;
}

bool is_threshold_mode(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_THRESHOLD;
}

bool is_start_end_level_mode(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_EDIT;
}

bool is_time_bpm_mode(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_IDLE && dev->time_bpm_mode;
}

bool is_delete_mode(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_DELETE_SELECT ||
           dev->sampling_state == SAMPLING_DELETE_ARMED ||
           dev->sampling_state == SAMPLING_TRUNCATE_ARMED ||
           dev->sampling_state == SAMPLING_DELETE_ALL_SELECT ||
           dev->sampling_state == SAMPLING_DELETE_ALL_ARMED ||
           dev->sampling_state == SAMPLING_SWAP_SELECT_A ||
           dev->sampling_state == SAMPLING_SWAP_SELECT_B;
}

bool is_resampling_mode(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_RESAMPLE_SOURCE ||
           dev->sampling_state == SAMPLING_RESAMPLE_DEST ||
           dev->sampling_state == SAMPLING_RESAMPLE_ARMED ||
           dev->sampling_state == SAMPLING_RESAMPLE_RECORDING;
}

bool is_resample_source_select(const Device* dev) {
    return dev && dev->sampling_state == SAMPLING_RESAMPLE_SOURCE;
}

bool is_resample_dest_select(const Device* dev) {
    return dev && dev->sampling_state == SAMPLING_RESAMPLE_DEST;
}

bool is_resample_armed(const Device* dev) {
    return dev && dev->sampling_state == SAMPLING_RESAMPLE_ARMED;
}

bool is_resample_recording(const Device* dev) {
    return dev && dev->sampling_state == SAMPLING_RESAMPLE_RECORDING;
}

int get_sampling_target_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->sampling_target_pad;
}

int get_resample_source_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->resample_source_pad;
}

int get_resample_dest_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->resample_dest_pad;
}

int get_last_sampling_target_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->last_sampling_target_pad;
}

int get_sample_level_threshold(const Device* dev) {
    if (!dev) return 0;
    return dev->sample_level_threshold;
}

int get_last_played_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->last_played_pad;
}

int get_time_bpm_pad(const Device* dev) {
    if (!dev) return -1;
    return dev->time_bpm_pad;
}

int get_pattern_bpm(const Device* dev) {
    if (!dev) return 120;
    return pattern_get_bpm(&dev->pattern_seq);
}

void set_pattern_bpm(Device* dev, int bpm) {
    if (!dev) return;
    pattern_set_bpm(&dev->pattern_seq, bpm);
}

int get_pattern_record_slot(const Device* dev) {
    if (!dev) return -1;
    return dev->pattern_record_slot;
}

int get_pattern_metronome_level(const Device* dev) {
    if (!dev) return 100;
    int slot = dev->pattern_record_slot;
    if (slot < 0 && pattern_is_playing(&dev->pattern_seq)) {
        slot = pattern_get_current_slot(&dev->pattern_seq);
    }
    if (slot < 0) {
        for (int i = 0; i < 32; ++i) {
            if (pattern_has_data(&dev->pattern_seq, i)) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) return 100;
    return pattern_get_metronome_level(&dev->pattern_seq, slot);
}

int consume_record_bpm_quantize(Device* dev) {
    if (!dev) return -1;
    int bpm = dev->pending_record_bpm_quantize;
    dev->pending_record_bpm_quantize = -1;
    return bpm;
}

int consume_mark_action(Device* dev) {
    if (!dev) return 0;
    int action = dev->mark_pending_action;
    dev->mark_pending_action = 0;
    return action;
}

bool consume_pattern_trigger(Device* dev, PatternProjectEvent* out) {
    if (!dev || !out) return false;
    PatternEvent ev{};
    if (!pattern_consume_trigger(&dev->pattern_seq, &ev)) return false;
    out->tick = ev.tick;
    out->sample_pad = ev.sample_pad;
    out->velocity = ev.velocity;
    return true;
}

int consume_pattern_metronome(Device* dev) {
    if (!dev) return 0;
    return pattern_consume_metronome(&dev->pattern_seq);
}

int get_mark_edit_pad(const Device* dev) {
    if (!dev) return -1;
    return (dev->mark_pending_action != 0) ? dev->mark_action_pad : dev->mark_edit_pad;
}

bool get_pad_loop_mode(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_loop_mode[pad_index];
}

bool get_pad_gate_mode(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_gate_mode[pad_index];
}

bool get_pad_reverse_mode(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_reverse_mode[pad_index];
}

void set_sampling_full(Device* dev) {
    if (!dev) return;
    dev->sampling_full = true;
    if (dev->sampling_state == SAMPLING_RECORDING ||
        dev->sampling_state == SAMPLING_RESAMPLE_RECORDING) {
        dev->sampling_state = SAMPLING_IDLE;
        dev->sampling_target_pad = -1;
        dev->resample_source_pad = -1;
        dev->resample_dest_pad = -1;
    }
}

void start_threshold_recording(Device* dev) {
    if (!dev || dev->sampling_state != SAMPLING_READY || dev->sampling_target_pad < 0) return;
    dev->sampling_state = SAMPLING_RECORDING;
    dev->sampling_full = false;
}

void finish_threshold_recording(Device* dev) {
    if (!dev) return;
    bool is_regular = (dev->sampling_state == SAMPLING_RECORDING);
    bool is_resample = (dev->sampling_state == SAMPLING_RESAMPLE_RECORDING);
    if (!is_regular && !is_resample) return;
    dev->sampling_state = SAMPLING_IDLE;
    if (dev->sampling_target_pad >= 0) {
        dev->pad_has_sample[dev->sampling_target_pad] = true;
        dev->pad_loop_mode[dev->sampling_target_pad] = false;
        dev->pad_gate_mode[dev->sampling_target_pad] = false;
        dev->pad_reverse_mode[dev->sampling_target_pad] = false;
        dev->last_sampling_target_pad = dev->sampling_target_pad;
        dev->last_played_pad = dev->sampling_target_pad;
        dev->pad_led_hold_frames[dev->sampling_target_pad] = 180;
        if (dev->bpm_armed_for_next_sample && dev->bpm_value >= 40 && dev->bpm_value <= 200) {
            dev->pending_record_bpm_quantize = dev->bpm_value;
            dev->bpm_armed_for_next_sample = false;
        }
    }
    dev->sampling_target_pad = -1;
    dev->resample_source_pad = -1;
    dev->resample_dest_pad = -1;
    dev->sampling_full = false;
    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
}

void note_pad_played(Device* dev, int pad_index, int velocity) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    if (dev->mark_end_only_lock_pad >= 0 && dev->mark_end_only_lock_pad != pad_index) {
        std::printf("[MARK] clear END_ONLY lock on retrigger -> old bank %c pad %d, new bank %c pad %d\n",
                    pad_bank_name(dev->mark_end_only_lock_pad), pad_bank_number(dev->mark_end_only_lock_pad),
                    pad_bank_name(pad_index), pad_bank_number(pad_index));
    }
    dev->mark_end_only_lock_pad = -1;
    dev->last_played_pad = pad_index;
    if (dev->pattern_recording) {
        pattern_record_pad_hit(&dev->pattern_seq, pad_index, std::clamp(velocity, 1, 127));
    }
}

void note_pattern_pad_played(Device* dev, int pad_index, int velocity) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    (void)velocity;
    if (dev->mark_end_only_lock_pad >= 0) {
        std::printf("[MARK] clear END_ONLY lock from pattern trigger -> old bank %c pad %d\n",
                    pad_bank_name(dev->mark_end_only_lock_pad), pad_bank_number(dev->mark_end_only_lock_pad));
    }
    dev->mark_end_only_lock_pad = -1;
    dev->last_played_pad = pad_index;
}

void set_edit_display_value(Device* dev, int value) {
    if (!dev) return;
    int n = std::clamp(value, 0, 999);
    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;
    dev->state.display.digit[0] = (hundreds > 0) ? SEG_DIGITS[hundreds] : SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SEG_DIGITS[tens]     : SEG_BLANK;
    dev->state.display.digit[2] = SEG_DIGITS[ones];
    dev->edit_display_locked = true;
}

int consume_deleted_pad(Device* dev) {
    if (!dev) return -1;
    int out = dev->deleted_pad_pending;
    dev->deleted_pad_pending = -1;
    return out;
}

int consume_truncate_pad(Device* dev) {
    if (!dev) return -1;
    int out = dev->truncate_pad_pending;
    dev->truncate_pad_pending = -1;
    return out;
}

int consume_delete_all_group(Device* dev) {
    if (!dev) return -1;
    int out = dev->delete_all_pending_group;
    dev->delete_all_pending_group = -1;
    return out;
}

bool consume_swap_pads(Device* dev, int* pad_a, int* pad_b) {
    if (!dev || !pad_a || !pad_b) return false;
    if (dev->swap_pending_a < 0 || dev->swap_pending_b < 0) return false;
    *pad_a = dev->swap_pending_a;
    *pad_b = dev->swap_pending_b;
    dev->swap_pending_a = -1;
    dev->swap_pending_b = -1;
    return true;
}

bool consume_memory_card_format(Device* dev) {
    if (!dev) return false;
    bool out = dev->memory_card_format_pending;
    dev->memory_card_format_pending = false;
    return out;
}

int consume_memory_card_sample_save(Device* dev) {
    if (!dev) return -1;
    int out = dev->memory_card_sample_save_pending_slot;
    dev->memory_card_sample_save_pending_slot = -1;
    return out;
}

int consume_memory_card_sample_load(Device* dev) {
    if (!dev) return -1;
    int out = dev->memory_card_sample_load_pending_slot;
    dev->memory_card_sample_load_pending_slot = -1;
    return out;
}

bool consume_memory_card_dirty(Device* dev) {
    if (!dev) return false;
    bool out = dev->memory_card_dirty;
    dev->memory_card_dirty = false;
    return out;
}

void set_mark_lit(Device* dev, bool lit) {
    if (!dev) return;
    dev->state.buttons[BTN_MARK].lit = lit;
}

void set_pad_playing(Device* dev, int pad_index, bool playing) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_is_playing[pad_index] = playing;
}

void set_pad_marked(Device* dev, int pad_index, bool marked) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_is_marked[pad_index] = marked;
}

bool get_sampling_stereo(const Device* dev) {
    if (!dev) return false;
    return dev->sampling_stereo;
}

SampleQuality get_sampling_quality(const Device* dev) {
    if (!dev) return SAMPLE_QUALITY_STANDARD;
    return dev->sampling_quality;
}

// ─── Display helpers ──────────────────────────────────────────────────────────

void display_number(Device* dev, int n) {
    if (!dev) return;
    n = std::clamp(n, 0, 999);

    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;

    dev->state.display.digit[0] = ((hundreds > 0) ? SEG_DIGITS[hundreds] : SEG_BLANK) | SEG_DP;
    dev->state.display.digit[1] = ((n >= 10)      ? SEG_DIGITS[tens]     : SEG_BLANK) | SEG_DP;
    dev->state.display.digit[2] = SEG_DIGITS[ones] | SEG_DP;
}

void display_raw(Device* dev, uint8_t d0, uint8_t d1, uint8_t d2) {
    if (!dev) return;
    dev->state.display.digit[0] = d0;
    dev->state.display.digit[1] = d1;
    dev->state.display.digit[2] = d2;
}

uint8_t char_to_seg(char c) {
    switch (c) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return SEG_DIGITS[c - '0'];
        case 'A': case 'a': return SEG_A|SEG_B|SEG_C|SEG_E|SEG_F|SEG_G;
        case 'b':           return SEG_C|SEG_D|SEG_E|SEG_F|SEG_G;
        case 'C':           return SEG_A|SEG_D|SEG_E|SEG_F;
        case 'd':           return SEG_B|SEG_C|SEG_D|SEG_E|SEG_G;
        case 'E':           return SEG_A|SEG_D|SEG_E|SEG_F|SEG_G;
        case 'F': case 'f': return SEG_A|SEG_E|SEG_F|SEG_G;
        case 'G':           return SEG_A|SEG_C|SEG_D|SEG_E|SEG_F;
        case 'H': case 'h': return SEG_B|SEG_C|SEG_E|SEG_F|SEG_G;
        case 'I': case 'i': return SEG_C;
        case 'L':           return SEG_D|SEG_E|SEG_F;
        case 'M':           return SEG_A|SEG_B|SEG_C|SEG_E|SEG_F;
        case 'n':           return SEG_C|SEG_E|SEG_G;
        case 'o':           return SEG_C|SEG_D|SEG_E|SEG_G;
        case 'P': case 'p': return SEG_A|SEG_B|SEG_E|SEG_F|SEG_G;
        case 'r':           return SEG_E|SEG_G;
        case 'S': case 's': return SEG_A|SEG_C|SEG_D|SEG_F|SEG_G;
        case 't':           return SEG_D|SEG_E|SEG_F|SEG_G;
        case 'u': case 'U':
        case 'V': case 'v': return SEG_C|SEG_D|SEG_E;
        case 'Y': case 'y': return SEG_B|SEG_C|SEG_D|SEG_F|SEG_G;
        case '-':           return SEG_G;
        default:            return SEG_BLANK;
    }
}

void display_str3(Device* dev, const char* label3) {
    if (!dev || !label3) return;
    uint8_t d0 = char_to_seg(label3[0]);
    uint8_t d1 = label3[1] ? char_to_seg(label3[1]) : SEG_BLANK;
    uint8_t d2 = label3[2] ? char_to_seg(label3[2]) : SEG_BLANK;
    display_raw(dev, d0, d1, d2);
}

// ─── Pad sample query ─────────────────────────────────────────────────────────

} // namespace sp303
