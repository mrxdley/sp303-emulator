#include "sp303.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

static constexpr uint32_t SP303_CORE_TAP_SAMPLE_RATE = 44100;

// ─── Metadata tables ──────────────────────────────────────────────────────────

const SP303ButtonDef SP303_BUTTON_DEFS[SP303_BTN_COUNT] = {
    { SP303_BTN_FILTER_DRIVE,    "FILTER+DRIVE",    nullptr,         nullptr       },
    { SP303_BTN_PITCH,           "PITCH",           nullptr,         nullptr       },
    { SP303_BTN_DELAY,           "DELAY",           nullptr,         nullptr       },
    { SP303_BTN_MFX,             "MFX",             nullptr,         nullptr       },
    { SP303_BTN_VINYL_SIM,       "VINYL SIM",       nullptr,         nullptr       },
    { SP303_BTN_ISOLATOR,        "ISOLATOR",        nullptr,         nullptr       },

    { SP303_BTN_START_END_LEVEL, "START/END/LEVEL", nullptr,         nullptr       },
    { SP303_BTN_TIME_BPM,        "TIME/BPM",        nullptr,         nullptr       },

    { SP303_BTN_PATTERN_SELECT,  "PATTERN SELECT",  nullptr,         nullptr       },
    { SP303_BTN_LENGTH,          "LENGTH",          nullptr,         nullptr       },
    { SP303_BTN_QUANTIZE,        "QUANTIZE",        nullptr,         nullptr       },

    { SP303_BTN_TAP_TEMPO,       "TAP TEMPO",       "EFFECT GRAB",   nullptr       },
    { SP303_BTN_CANCEL,          "CANCEL",          "PATTERN STOP",  nullptr       },
    { SP303_BTN_REMAIN,          "REMAIN",          "CURRENT PAD",   nullptr       },

    { SP303_BTN_LONG_LOFI,       "LONG",            "LOFI",          nullptr       },
    { SP303_BTN_STEREO,          "STEREO",          nullptr,         nullptr       },
    { SP303_BTN_GATE,            "GATE",            nullptr,         nullptr       },
    { SP303_BTN_LOOP,            "LOOP",            nullptr,         nullptr       },
    { SP303_BTN_REVERSE,         "REVERSE",         nullptr,         nullptr       },

    { SP303_BTN_DEL,             "DEL",             nullptr,         nullptr       },
    { SP303_BTN_REC,             "REC",             "PATTERN REC",   nullptr       },
    { SP303_BTN_RESAMPLE,        "RE-SAMPLE",       nullptr,         nullptr       },
    { SP303_BTN_MARK,            "MARK",            "START POINT",   "END POINT"   },

    { SP303_BTN_BANK_A,          "BANK A",          nullptr,         nullptr       },
    { SP303_BTN_BANK_B,          "BANK B",          nullptr,         nullptr       },
    { SP303_BTN_BANK_C,          "BANK C",          nullptr,         nullptr       },
    { SP303_BTN_BANK_D,          "BANK D",          nullptr,         nullptr       },

    // Pads — all 32 (banks A–D × 8).
    // Labels are "1"–"8" per bank; the renderer uses active_bank to pick the right set.
    { SP303_BTN_PAD_1,           "1",               nullptr,         nullptr       },
    { SP303_BTN_PAD_2,           "2",               nullptr,         nullptr       },
    { SP303_BTN_PAD_3,           "3",               nullptr,         nullptr       },
    { SP303_BTN_PAD_4,           "4",               nullptr,         nullptr       },
    { SP303_BTN_PAD_5,           "5",               nullptr,         nullptr       },
    { SP303_BTN_PAD_6,           "6",               nullptr,         nullptr       },
    { SP303_BTN_PAD_7,           "7",               nullptr,         nullptr       },
    { SP303_BTN_PAD_8,           "8",               nullptr,         nullptr       },
    { SP303_BTN_PAD_9,           "1",               nullptr,         nullptr       },
    { SP303_BTN_PAD_10,          "2",               nullptr,         nullptr       },
    { SP303_BTN_PAD_11,          "3",               nullptr,         nullptr       },
    { SP303_BTN_PAD_12,          "4",               nullptr,         nullptr       },
    { SP303_BTN_PAD_13,          "5",               nullptr,         nullptr       },
    { SP303_BTN_PAD_14,          "6",               nullptr,         nullptr       },
    { SP303_BTN_PAD_15,          "7",               nullptr,         nullptr       },
    { SP303_BTN_PAD_16,          "8",               nullptr,         nullptr       },
    { SP303_BTN_PAD_17,          "1",               nullptr,         nullptr       },
    { SP303_BTN_PAD_18,          "2",               nullptr,         nullptr       },
    { SP303_BTN_PAD_19,          "3",               nullptr,         nullptr       },
    { SP303_BTN_PAD_20,          "4",               nullptr,         nullptr       },
    { SP303_BTN_PAD_21,          "5",               nullptr,         nullptr       },
    { SP303_BTN_PAD_22,          "6",               nullptr,         nullptr       },
    { SP303_BTN_PAD_23,          "7",               nullptr,         nullptr       },
    { SP303_BTN_PAD_24,          "8",               nullptr,         nullptr       },
    { SP303_BTN_PAD_25,          "1",               nullptr,         nullptr       },
    { SP303_BTN_PAD_26,          "2",               nullptr,         nullptr       },
    { SP303_BTN_PAD_27,          "3",               nullptr,         nullptr       },
    { SP303_BTN_PAD_28,          "4",               nullptr,         nullptr       },
    { SP303_BTN_PAD_29,          "5",               nullptr,         nullptr       },
    { SP303_BTN_PAD_30,          "6",               nullptr,         nullptr       },
    { SP303_BTN_PAD_31,          "7",               nullptr,         nullptr       },
    { SP303_BTN_PAD_32,          "8",               nullptr,         nullptr       },

    { SP303_BTN_HOLD,            "HOLD",            nullptr,         nullptr       },
    { SP303_BTN_EXT_SOURCE,      "EXT SOURCE",      nullptr,         nullptr       },
};

const SP303KnobDef SP303_KNOB_DEFS[SP303_KNOB_COUNT] = {
    { SP303_KNOB_VOLUME,    "VOLUME",    nullptr,  nullptr  },
    { SP303_KNOB_CUTOFF,    "CUTOFF",    "TIME",   "START"  },
    { SP303_KNOB_RESONANCE, "RESONANCE", "BPM",    "END"    },
    { SP303_KNOB_DRIVE,     "DRIVE",     "LEVEL",  nullptr  },
};

// ─── 7-Segment encoding ───────────────────────────────────────────────────────
//
// Bit layout:  7   6   5   4   3   2   1   0
//             dp   g   f   e   d   c   b   a

const uint8_t SP303_SEG_DIGITS[10] = {
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

const uint8_t SP303_SEG_BLANK = 0x00;
const uint8_t SP303_SEG_DASH  = SP303_SEG_G; // 0x40 — middle bar only

// "Err": E(0x79), r(0x50), r(0x50)
const uint8_t SP303_SEG_ERR[3] = {
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // E
    SP303_SEG_E | SP303_SEG_G,                                             // r
    SP303_SEG_E | SP303_SEG_G,                                             // r
};

// "FuL" for memory full: F(0x71), u(0x1C), L(0x38)
const uint8_t SP303_SEG_FUL[3] = {
    SP303_SEG_A | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G,                 // F
    SP303_SEG_C | SP303_SEG_D | SP303_SEG_E,                               // u
    SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,                               // L
};

// "rEC" for recording: r(0x50), E(0x79), C(0x39)
const uint8_t SP303_SEG_REC[3] = {
    SP303_SEG_E | SP303_SEG_G,                                             // r
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // E
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,               // C
};

// "rdY": r(0x50), d(0x5E), Y(0x6E)
const uint8_t SP303_SEG_RDY[3] = {
    SP303_SEG_E | SP303_SEG_G,                                             // r
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_D | SP303_SEG_E | SP303_SEG_G, // d
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_D | SP303_SEG_F | SP303_SEG_G, // Y
};

// "Edt": E(0x79), d(0x5E), t(0x78)
const uint8_t SP303_SEG_EDT[3] = {
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // E
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_D | SP303_SEG_E | SP303_SEG_G, // d
    SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G,               // t
};

// "dEL": d(0x5E), E(0x79), L(0x38)
const uint8_t SP303_SEG_DEL[3] = {
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_D | SP303_SEG_E | SP303_SEG_G, // d
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // E
    SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,                               // L
};

// "trC": t(0x78), r(0x50), C(0x39)
const uint8_t SP303_SEG_TRC[3] = {
    SP303_SEG_D | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G,               // t
    SP303_SEG_E | SP303_SEG_G,                                             // r
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,               // C
};

// "dAL": d(0x5E), A(0x77), L(0x38)
const uint8_t SP303_SEG_DAL[3] = {
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_D | SP303_SEG_E | SP303_SEG_G, // d
    SP303_SEG_A | SP303_SEG_B | SP303_SEG_C | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // A
    SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,                               // L
};

// "CHG": C(0x39), H(0x76), G(0x3D-ish lower-case style)
const uint8_t SP303_SEG_CHG[3] = {
    SP303_SEG_A | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F,               // C
    SP303_SEG_B | SP303_SEG_C | SP303_SEG_E | SP303_SEG_F | SP303_SEG_G, // H
    SP303_SEG_A | SP303_SEG_C | SP303_SEG_D | SP303_SEG_E | SP303_SEG_F, // G
};

// ─── Sampling state machine ───────────────────────────────────────────────────

typedef enum {
    SAMPLING_IDLE,      // Normal operation
    SAMPLING_STANDBY,   // REC pressed, waiting for pad selection
    SAMPLING_READY,     // Waiting for input to cross the sample threshold
    SAMPLING_RECORDING, // Recording in progress
    SAMPLING_THRESHOLD, // Editing the sample-level trigger threshold
    SAMPLING_EDIT,      // START/END/LEVEL edit mode for last played pad
    SAMPLING_DELETE_SELECT, // DEL pressed, choosing pad
    SAMPLING_DELETE_ARMED,  // delete target selected, waiting confirm
    SAMPLING_TRUNCATE_ARMED, // DEL+MARK path waiting DEL confirm
    SAMPLING_DELETE_ALL_SELECT, // CANCEL+DEL delete-all select
    SAMPLING_DELETE_ALL_ARMED,  // delete-all memory target chosen
    SAMPLING_SWAP_SELECT_A,     // DEL+REC: choose first pad
    SAMPLING_SWAP_SELECT_B,     // first pad selected: choose second + confirm REC
} SamplingState;

// ─── Internal device state ────────────────────────────────────────────────────

struct SP303Device {
    SP303State state;

    // Internal clock accumulator (samples)
    uint32_t sample_clock;

    // ─── Sampling state ───────────────────────────────────────────────────────
    SamplingState sampling_state;
    int sampling_target_pad;      // -1 = not selected yet, 0-31 = pad index
    int last_sampling_target_pad; // persists after recording stops (for audio assignment)
    bool sampling_full;           // FuL condition
    int sample_level_threshold;   // 0-8 trigger level for auto record start/stop
    int last_played_pad;          // -1 if none
    int pad_led_hold_frames[32];  // keep played pads lit briefly in idle/edit
    bool edit_display_locked;     // show last edited numeric value while in edit mode
    int delete_target_pad;        // selected pad for delete confirm
    int deleted_pad_pending;      // renderer consumes and clears audio slot
    int truncate_pad_pending;     // renderer consumes and truncates audio slot
    int delete_all_group;         // -1 none, 0=A/B (internal), 1=C/D (card)
    int delete_all_pending_group; // renderer consumes and clears audio slots
    int swap_pad_a;               // first selected pad for CHG mode
    int swap_pad_b;               // second selected pad for CHG mode
    int swap_pending_a;           // pending swap pair for renderer/audio
    int swap_pending_b;
    bool sampling_stereo;         // false=mono, true=stereo
    SP303SampleQuality sampling_quality;
    bool bpm_edit_mode;           // TIME/BPM lit in standby
    int bpm_value;                // -1 (cleared) or 40..200
    bool bpm_armed_for_next_sample; // quantize next completed recording
    uint32_t tap_times[4];        // sample_clock timestamps
    int tap_count;                // 0..4
    int pending_record_bpm_quantize; // -1 none, else 40..200
    int rec_gain_display_frames;  // temporary gain display while recording
    int rec_gain_value;           // 0..127 display value
    bool pad_loop_mode[32];       // true=loop, false=one-shot
    bool pad_gate_mode[32];       // true=gate, false=trigger

    // ─── Pad state ────────────────────────────────────────────────────────────
    // Each pad can have a sample assigned (true) or be empty (false)
    bool pad_has_sample[32];

    // ─── Blink timing ─────────────────────────────────────────────────────────
    uint32_t blink_phase;         // 0-59 (60 phases = ~1 second at 60fps)
    bool blink_on;                // true during "on" part of blink
};

void sp303_finish_threshold_recording(SP303Device* dev);

// ─── Lifecycle ────────────────────────────────────────────────────────────────

SP303Device* sp303_create(void) {
    SP303Device* dev = static_cast<SP303Device*>(std::calloc(1, sizeof(SP303Device)));
    if (!dev) return nullptr;

    // All buttons off, all knobs at zero
    std::memset(&dev->state, 0, sizeof(SP303State));
    dev->sample_clock = 0;

    // Default input gain (CTRL 3/MFX DRIVE knob) - 80% for reasonable recording level
    dev->state.knobs[SP303_KNOB_DRIVE].value = 0.8f;
    dev->state.knobs[SP303_KNOB_VOLUME].value = 1.0f;

    // Power-on: blank display
    dev->state.display.digit[0] = SP303_SEG_BLANK;
    dev->state.display.digit[1] = SP303_SEG_BLANK;
    dev->state.display.digit[2] = SP303_SEG_BLANK;

    // Default bank: A
    dev->state.active_bank = 0;
    dev->state.buttons[SP303_BTN_BANK_A].lit = true;

    // Sampling state
    dev->sampling_state = SAMPLING_IDLE;
    dev->sampling_target_pad = -1;
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
    dev->sampling_stereo = false;
    dev->sampling_quality = SP303_SAMPLE_QUALITY_STANDARD;
    dev->bpm_edit_mode = false;
    dev->bpm_value = -1;
    dev->bpm_armed_for_next_sample = false;
    std::memset(dev->tap_times, 0, sizeof(dev->tap_times));
    dev->tap_count = 0;
    dev->pending_record_bpm_quantize = -1;
    dev->rec_gain_display_frames = 0;
    dev->rec_gain_value = 0;

    // All pads empty
    std::memset(dev->pad_has_sample, 0, sizeof(dev->pad_has_sample));
    std::memset(dev->pad_led_hold_frames, 0, sizeof(dev->pad_led_hold_frames));
    std::memset(dev->pad_loop_mode, 0, sizeof(dev->pad_loop_mode));
    std::memset(dev->pad_gate_mode, 0, sizeof(dev->pad_gate_mode));

    // Blink timing
    dev->blink_phase = 0;
    dev->blink_on = false;

    return dev;
}

void sp303_destroy(SP303Device* dev) {
    std::free(dev);
}

// ─── Helper: Get first empty pad in current bank ──────────────────────────────

static int find_empty_pad(SP303Device* dev) {
    int bank_start = dev->state.active_bank * 8;
    for (int i = 0; i < 8; ++i) {
        int pad = bank_start + i;
        if (!dev->pad_has_sample[pad]) return pad;
    }
    return -1; // No empty pad in this bank
}

// ─── Helper: Get pad ID from bank + physical pad index ────────────────────────

static int get_pad_button_id(int pad_index) {
    return SP303_BTN_PAD_1 + pad_index; // 0-31 maps to PAD_1 through PAD_32
}

static void set_display_number_plain(SP303Device* dev, int value) {
    int n = std::clamp(value, 0, 999);
    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;
    dev->state.display.digit[0] = (hundreds > 0) ? SP303_SEG_DIGITS[hundreds] : SP303_SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SP303_SEG_DIGITS[tens]     : SP303_SEG_BLANK;
    dev->state.display.digit[2] = SP303_SEG_DIGITS[ones];
}

// ─── Input ────────────────────────────────────────────────────────────────────

void sp303_button_down(SP303Device* dev, SP303ButtonID btn) {
    if (!dev || btn < 0 || btn >= SP303_BTN_COUNT) return;
    dev->state.buttons[btn].pressed = true;

    if (dev->sampling_state == SAMPLING_EDIT) {
        int last_pad_btn = (dev->last_played_pad >= 0)
            ? (SP303_BTN_PAD_1 + dev->last_played_pad)
            : -1;

        if (btn == SP303_BTN_START_END_LEVEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->edit_display_locked = false;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }

        // In Edt mode, retriggering the same pad stays in Edt.
        // Any other input exits Edt.
        if (btn == last_pad_btn) {
            return;
        }

        dev->sampling_state = SAMPLING_IDLE;
        dev->edit_display_locked = false;
        sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
    }

    if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        if (btn == SP303_BTN_DEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->delete_target_pad = -1;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        if (btn == SP303_BTN_MARK) {
            if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
                dev->pad_has_sample[dev->last_played_pad]) {
                dev->sampling_state = SAMPLING_TRUNCATE_ARMED;
                sp303_display_raw(dev, SP303_SEG_TRC[0], SP303_SEG_TRC[1], SP303_SEG_TRC[2]);
            }
            return;
        }
        if (btn >= SP303_BTN_PAD_1 && btn <= SP303_BTN_PAD_8) {
            int physical_pad = btn - SP303_BTN_PAD_1;
            int actual_pad = dev->state.active_bank * 8 + physical_pad;
            if (dev->pad_has_sample[actual_pad]) {
                dev->delete_target_pad = actual_pad;
                dev->sampling_state = SAMPLING_DELETE_ARMED;
            }
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        dev->delete_target_pad = -1;
        sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_DELETE_ALL_SELECT ||
               dev->sampling_state == SAMPLING_DELETE_ALL_ARMED) {
        if (btn >= SP303_BTN_BANK_A && btn <= SP303_BTN_BANK_D) {
            dev->delete_all_group = (btn <= SP303_BTN_BANK_B) ? 0 : 1;
            dev->sampling_state = SAMPLING_DELETE_ALL_ARMED;
            return;
        }
        if (btn == SP303_BTN_DEL &&
            dev->sampling_state == SAMPLING_DELETE_ALL_ARMED &&
            dev->delete_all_group >= 0) {
            int start = (dev->delete_all_group == 0) ? 0 : 16;
            int end = start + 16;
            for (int i = start; i < end; ++i) {
                dev->pad_has_sample[i] = false;
                dev->pad_led_hold_frames[i] = 0;
                dev->pad_loop_mode[i] = false;
                dev->pad_gate_mode[i] = false;
            }
            if (dev->last_played_pad >= start && dev->last_played_pad < end) {
                dev->last_played_pad = -1;
            }
            dev->delete_all_pending_group = dev->delete_all_group;
            dev->delete_all_group = -1;
            dev->sampling_state = SAMPLING_IDLE;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        return;
    } else if (dev->sampling_state == SAMPLING_TRUNCATE_ARMED) {
        if (btn == SP303_BTN_DEL) {
            if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
                dev->pad_has_sample[dev->last_played_pad]) {
                dev->truncate_pad_pending = dev->last_played_pad;
            }
            dev->sampling_state = SAMPLING_IDLE;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_DELETE_ARMED) {
        if (btn == SP303_BTN_DEL) {
            if (dev->delete_target_pad >= 0 && dev->delete_target_pad < 32) {
                dev->pad_has_sample[dev->delete_target_pad] = false;
                dev->pad_loop_mode[dev->delete_target_pad] = false;
                dev->pad_gate_mode[dev->delete_target_pad] = false;
                if (dev->last_played_pad == dev->delete_target_pad) {
                    dev->last_played_pad = -1;
                }
                dev->deleted_pad_pending = dev->delete_target_pad;
            }
            dev->sampling_state = SAMPLING_IDLE;
            dev->delete_target_pad = -1;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        dev->sampling_state = SAMPLING_IDLE;
        dev->delete_target_pad = -1;
        sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
    } else if (dev->sampling_state == SAMPLING_SWAP_SELECT_A ||
               dev->sampling_state == SAMPLING_SWAP_SELECT_B) {
        if (btn >= SP303_BTN_PAD_1 && btn <= SP303_BTN_PAD_8) {
            int physical_pad = btn - SP303_BTN_PAD_1;
            int actual_pad = dev->state.active_bank * 8 + physical_pad;
            if (dev->sampling_state == SAMPLING_SWAP_SELECT_A) {
                dev->swap_pad_a = actual_pad;
                dev->swap_pad_b = -1;
                dev->sampling_state = SAMPLING_SWAP_SELECT_B;
            } else {
                dev->swap_pad_b = actual_pad;
            }
            return;
        }
        if (btn == SP303_BTN_REC &&
            dev->sampling_state == SAMPLING_SWAP_SELECT_B &&
            dev->swap_pad_a >= 0 && dev->swap_pad_b >= 0 &&
            dev->swap_pad_a != dev->swap_pad_b) {
            int a = dev->swap_pad_a;
            int b = dev->swap_pad_b;
            std::swap(dev->pad_has_sample[a], dev->pad_has_sample[b]);
            std::swap(dev->pad_loop_mode[a], dev->pad_loop_mode[b]);
            std::swap(dev->pad_gate_mode[a], dev->pad_gate_mode[b]);
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
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        if (btn == SP303_BTN_CANCEL) {
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            dev->sampling_state = SAMPLING_IDLE;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }
        return;
    }

    // Bank switch: light the pressed bank, unlight the others, update active_bank
    if (btn >= SP303_BTN_BANK_A && btn <= SP303_BTN_BANK_D) {
        uint8_t bank = static_cast<uint8_t>(btn - SP303_BTN_BANK_A);
        dev->state.active_bank = bank;
        for (int b = 0; b < 4; ++b)
            dev->state.buttons[SP303_BTN_BANK_A + b].lit = (b == bank);
        return;
    }

    if (btn == SP303_BTN_TIME_BPM) {
        if (dev->sampling_state == SAMPLING_STANDBY) {
            dev->bpm_edit_mode = !dev->bpm_edit_mode;
            dev->tap_count = 0;
            if (!dev->bpm_edit_mode) {
                dev->bpm_armed_for_next_sample = (dev->bpm_value >= 40 && dev->bpm_value <= 200);
            }
        }
        return;
    }

    if (btn == SP303_BTN_TAP_TEMPO &&
        dev->sampling_state == SAMPLING_STANDBY &&
        dev->bpm_edit_mode) {
        if (dev->tap_count < 4) {
            dev->tap_times[dev->tap_count++] = dev->sample_clock;
        } else {
            dev->tap_times[0] = dev->tap_times[1];
            dev->tap_times[1] = dev->tap_times[2];
            dev->tap_times[2] = dev->tap_times[3];
            dev->tap_times[3] = dev->sample_clock;
        }
        if (dev->tap_count >= 4) {
            uint32_t d1 = dev->tap_times[1] - dev->tap_times[0];
            uint32_t d2 = dev->tap_times[2] - dev->tap_times[1];
            uint32_t d3 = dev->tap_times[3] - dev->tap_times[2];
            float avg = (d1 + d2 + d3) / 3.0f;
            if (avg > 1.0f) {
                int bpm = (int)std::lround((60.0f * SP303_CORE_TAP_SAMPLE_RATE) / avg);
                dev->bpm_value = std::clamp(bpm, 40, 200);
            }
        }
        return;
    }

    // ─── REC button: sampling workflow ────────────────────────────────────────
    if (btn == SP303_BTN_REC) {
        if (dev->sampling_state == SAMPLING_THRESHOLD) {
            dev->sampling_state = SAMPLING_IDLE;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            return;
        }

        // DEL + REC enters sample assignment exchange mode.
        // If DEL was pressed first, we may already be in DELETE_SELECT.
        if ((dev->sampling_state == SAMPLING_IDLE || dev->sampling_state == SAMPLING_DELETE_SELECT) &&
            dev->state.buttons[SP303_BTN_DEL].pressed) {
            dev->sampling_state = SAMPLING_SWAP_SELECT_A;
            dev->delete_target_pad = -1;
            dev->swap_pad_a = -1;
            dev->swap_pad_b = -1;
            sp303_display_raw(dev, SP303_SEG_CHG[0], SP303_SEG_CHG[1], SP303_SEG_CHG[2]);
            return;
        }

        if (dev->sampling_state == SAMPLING_IDLE) {
            if (dev->state.buttons[SP303_BTN_CANCEL].pressed) {
                dev->sampling_state = SAMPLING_THRESHOLD;
                return;
            }
            // Enter sampling standby mode
            dev->sampling_state = SAMPLING_STANDBY;
            dev->sampling_target_pad = -1;
            dev->sampling_full = false;
            dev->bpm_edit_mode = false;
            dev->tap_count = 0;
            dev->bpm_armed_for_next_sample = false;
            // Don't auto-select - user will select by pressing a pad
        } else if (dev->sampling_state == SAMPLING_STANDBY) {
            if (dev->bpm_edit_mode) {
                dev->bpm_edit_mode = false;
                dev->tap_count = 0;
                dev->bpm_armed_for_next_sample = (dev->bpm_value >= 40 && dev->bpm_value <= 200);
            }
            // Arm threshold-based recording if a pad is selected
            if (dev->sampling_target_pad >= 0) {
                dev->sampling_state = SAMPLING_READY;
            }
        } else if (dev->sampling_state == SAMPLING_READY) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->sampling_target_pad = -1;
            dev->sampling_full = false;
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
        } else if (dev->sampling_state == SAMPLING_RECORDING) {
            // Stop recording and assign to the selected pad
            sp303_finish_threshold_recording(dev);
        }
        return;
    }

    if (btn == SP303_BTN_DEL) {
        if (dev->sampling_state == SAMPLING_IDLE) {
            if (dev->state.buttons[SP303_BTN_CANCEL].pressed) {
                dev->sampling_state = SAMPLING_DELETE_ALL_SELECT;
                dev->delete_all_group = -1;
                sp303_display_raw(dev, SP303_SEG_DAL[0], SP303_SEG_DAL[1], SP303_SEG_DAL[2]);
                return;
            }
            dev->sampling_state = SAMPLING_DELETE_SELECT;
            dev->delete_target_pad = -1;
            sp303_display_raw(dev, SP303_SEG_DEL[0], SP303_SEG_DEL[1], SP303_SEG_DEL[2]);
        }
        return;
    }

    if (btn == SP303_BTN_START_END_LEVEL) {
        if (dev->sampling_state == SAMPLING_IDLE && dev->last_played_pad >= 0) {
            dev->sampling_state = SAMPLING_EDIT;
            dev->edit_display_locked = false;
        }
        return;
    }

    if (btn == SP303_BTN_LOOP) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad]) {
            dev->pad_loop_mode[dev->last_played_pad] = !dev->pad_loop_mode[dev->last_played_pad];
        }
        return;
    }

    if (btn == SP303_BTN_GATE) {
        if (dev->sampling_state == SAMPLING_IDLE &&
            dev->last_played_pad >= 0 &&
            dev->last_played_pad < 32 &&
            dev->pad_has_sample[dev->last_played_pad]) {
            dev->pad_gate_mode[dev->last_played_pad] = !dev->pad_gate_mode[dev->last_played_pad];
        }
        return;
    }

    if (btn == SP303_BTN_STEREO) {
        dev->sampling_stereo = !dev->sampling_stereo;
        return;
    }

    if (btn == SP303_BTN_LONG_LOFI) {
        if (dev->sampling_quality == SP303_SAMPLE_QUALITY_STANDARD) {
            dev->sampling_quality = SP303_SAMPLE_QUALITY_LONG;
        } else if (dev->sampling_quality == SP303_SAMPLE_QUALITY_LONG) {
            dev->sampling_quality = SP303_SAMPLE_QUALITY_LOFI;
        } else {
            dev->sampling_quality = SP303_SAMPLE_QUALITY_STANDARD;
        }
        return;
    }

    // ─── CANCEL button: exit sampling mode ────────────────────────────────────
    if (btn == SP303_BTN_CANCEL) {
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
            sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
        }
        return;
    }

    // ─── Pad selection during sampling standby ────────────────────────────────
    if (dev->sampling_state == SAMPLING_STANDBY) {
        if (btn >= SP303_BTN_PAD_1 && btn <= SP303_BTN_PAD_8) {
            // Map physical pad to actual pad in current bank
            int physical_pad = btn - SP303_BTN_PAD_1;
            int actual_pad = dev->state.active_bank * 8 + physical_pad;
            dev->sampling_target_pad = actual_pad;
            return;
        }
    }
}

void sp303_button_up(SP303Device* dev, SP303ButtonID btn) {
    if (!dev || btn < 0 || btn >= SP303_BTN_COUNT) return;
    dev->state.buttons[btn].pressed = false;
}

void sp303_knob_set(SP303Device* dev, SP303KnobID knob, float value) {
    if (!dev || knob < 0 || knob >= SP303_KNOB_COUNT) return;
    float clamped = std::clamp(value, 0.0f, 1.0f);
    dev->state.knobs[knob].value = clamped;

    if (knob == SP303_KNOB_RESONANCE &&
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

    if (knob == SP303_KNOB_DRIVE && dev->sampling_state == SAMPLING_THRESHOLD) {
        dev->sample_level_threshold = std::clamp((int)std::lround(clamped * 8.0f), 0, 8);
    }
    if (knob == SP303_KNOB_DRIVE && dev->sampling_state == SAMPLING_RECORDING) {
        dev->rec_gain_value = std::clamp((int)std::lround(clamped * 127.0f), 0, 127);
        dev->rec_gain_display_frames = 30;
    }
}

void sp303_indicator_set(SP303Device* dev, SP303IndicatorID ind, bool lit) {
    if (!dev || ind < 0 || ind >= SP303_IND_COUNT) return;
    dev->state.indicators[ind].lit = lit;
}

// ─── Clock ────────────────────────────────────────────────────────────────────

void sp303_tick(SP303Device* dev, uint32_t samples_elapsed) {
    if (!dev) return;
    dev->sample_clock += samples_elapsed;

    // Update blink phase (assuming ~735 samples = 1/60th second at 44.1kHz)
    // We'll just increment every tick call and wrap at 30 (0.5 sec on, 0.5 sec off)
    dev->blink_phase = (dev->blink_phase + 1) % 60;
    dev->blink_on = (dev->blink_phase < 30);

    for (int i = 0; i < 32; ++i) {
        if (dev->pad_led_hold_frames[i] > 0) dev->pad_led_hold_frames[i]--;
    }
    if (dev->rec_gain_display_frames > 0) dev->rec_gain_display_frames--;

    // ─── Update LED states based on sampling state ────────────────────────────

    // First, turn off all pad LEDs and REC LED (they'll be set based on state)
    dev->state.buttons[SP303_BTN_REC].lit = false;
    dev->state.buttons[SP303_BTN_START_END_LEVEL].lit = false;
    dev->state.buttons[SP303_BTN_TIME_BPM].lit = false;
    dev->state.buttons[SP303_BTN_DEL].lit = false;
    dev->state.buttons[SP303_BTN_GATE].lit = false;
    dev->state.buttons[SP303_BTN_LOOP].lit = false;
    dev->state.buttons[SP303_BTN_STEREO].lit = false;
    dev->state.buttons[SP303_BTN_LONG_LOFI].lit = false;
    for (int i = 0; i < 32; ++i) {
        dev->state.buttons[SP303_BTN_PAD_1 + i].lit = false;
    }

    if (dev->sampling_state == SAMPLING_IDLE) {
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_led_hold_frames[bank_start + i] > 0) {
                dev->state.buttons[SP303_BTN_PAD_1 + i].lit = true;
            }
        }
    }
    else if (dev->sampling_state == SAMPLING_THRESHOLD) {
        dev->state.buttons[SP303_BTN_REC].lit = dev->blink_on;
        sp303_display_raw(dev,
                          SP303_SEG_DASH,
                          SP303_SEG_DIGITS[dev->sample_level_threshold],
                          SP303_SEG_DASH);
    }
    else if (dev->sampling_state == SAMPLING_STANDBY) {
        // REC blinks while waiting to arm threshold-triggered recording
        dev->state.buttons[SP303_BTN_REC].lit = dev->blink_on;
        dev->state.buttons[SP303_BTN_TIME_BPM].lit = dev->bpm_edit_mode;

        int bank_start = dev->state.active_bank * 8;

        if (dev->sampling_target_pad >= 0) {
            // Pad selected: selected one solid, others off
            int selected_local = dev->sampling_target_pad - bank_start;
            for (int i = 0; i < 8; ++i) {
                if (i == selected_local) {
                    dev->state.buttons[SP303_BTN_PAD_1 + i].lit = true;
                } else {
                    dev->state.buttons[SP303_BTN_PAD_1 + i].lit = false;
                }
            }
            if (dev->bpm_edit_mode) {
                if (dev->bpm_value >= 40 && dev->bpm_value <= 200) {
                    set_display_number_plain(dev, dev->bpm_value);
                } else {
                    sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
                }
            } else {
                // Keep showing "---" even when pad selected (until recording starts)
                sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            }
        } else {
            // No pad selected: all empty pads blink
            for (int i = 0; i < 8; ++i) {
                if (!dev->pad_has_sample[bank_start + i]) {
                    dev->state.buttons[SP303_BTN_PAD_1 + i].lit = dev->blink_on;
                }
            }
            if (dev->bpm_edit_mode) {
                if (dev->bpm_value >= 40 && dev->bpm_value <= 200) {
                    set_display_number_plain(dev, dev->bpm_value);
                } else {
                    sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
                }
            } else {
                // Show "---" to indicate selection needed
                sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
            }
        }
    }
    else if (dev->sampling_state == SAMPLING_READY) {
        dev->state.buttons[SP303_BTN_REC].lit = true;
        if (dev->sampling_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->sampling_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[SP303_BTN_PAD_1 + local_pad].lit = true;
            }
        }
        sp303_display_raw(dev, SP303_SEG_RDY[0], SP303_SEG_RDY[1], SP303_SEG_RDY[2]);
    }
    else if (dev->sampling_state == SAMPLING_RECORDING) {
        // REC blinks
        dev->state.buttons[SP303_BTN_REC].lit = dev->blink_on;

        // Selected pad is lit solid
        if (dev->sampling_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->sampling_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[SP303_BTN_PAD_1 + local_pad].lit = true;
            }
        }

        // Check for FuL condition (would be set by audio engine)
        if (dev->sampling_full) {
            sp303_display_raw(dev, SP303_SEG_FUL[0], SP303_SEG_FUL[1], SP303_SEG_FUL[2]);
        } else if (dev->rec_gain_display_frames > 0) {
            set_display_number_plain(dev, dev->rec_gain_value);
        } else {
            // Show "rEC" while recording
            sp303_display_raw(dev, SP303_SEG_REC[0], SP303_SEG_REC[1], SP303_SEG_REC[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_EDIT) {
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_led_hold_frames[bank_start + i] > 0) {
                dev->state.buttons[SP303_BTN_PAD_1 + i].lit = true;
            }
        }
        dev->state.buttons[SP303_BTN_START_END_LEVEL].lit = true;
        if (dev->edit_display_locked) {
            // keep the temporary numeric readout set by sp303_set_edit_display_value()
        } else {
            sp303_display_raw(dev, SP303_SEG_EDT[0], SP303_SEG_EDT[1], SP303_SEG_EDT[2]);
        }
    }
    else if (dev->sampling_state == SAMPLING_SWAP_SELECT_A) {
        dev->state.buttons[SP303_BTN_DEL].lit = true;
        dev->state.buttons[SP303_BTN_REC].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[SP303_BTN_PAD_1 + i].lit = dev->blink_on;
            }
        }
        sp303_display_raw(dev, SP303_SEG_CHG[0], SP303_SEG_CHG[1], SP303_SEG_CHG[2]);
    }
    else if (dev->sampling_state == SAMPLING_SWAP_SELECT_B) {
        dev->state.buttons[SP303_BTN_DEL].lit = true;
        int bank_start = dev->state.active_bank * 8;
        int local_a = (dev->swap_pad_a >= 0) ? (dev->swap_pad_a - bank_start) : -1;
        int local_b = (dev->swap_pad_b >= 0) ? (dev->swap_pad_b - bank_start) : -1;

        if (dev->swap_pad_b >= 0 && dev->swap_pad_b != dev->swap_pad_a) {
            dev->state.buttons[SP303_BTN_REC].lit = dev->blink_on;
            if (local_a >= 0 && local_a < 8) dev->state.buttons[SP303_BTN_PAD_1 + local_a].lit = true;
            if (local_b >= 0 && local_b < 8) dev->state.buttons[SP303_BTN_PAD_1 + local_b].lit = true;
        } else {
            dev->state.buttons[SP303_BTN_REC].lit = true;
            for (int i = 0; i < 8; ++i) {
                if (dev->pad_has_sample[bank_start + i]) {
                    dev->state.buttons[SP303_BTN_PAD_1 + i].lit = dev->blink_on;
                }
            }
            if (local_a >= 0 && local_a < 8) dev->state.buttons[SP303_BTN_PAD_1 + local_a].lit = true;
        }
        sp303_display_raw(dev, SP303_SEG_CHG[0], SP303_SEG_CHG[1], SP303_SEG_CHG[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        dev->state.buttons[SP303_BTN_DEL].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[SP303_BTN_PAD_1 + i].lit = dev->blink_on;
            }
        }
        sp303_display_raw(dev, SP303_SEG_DEL[0], SP303_SEG_DEL[1], SP303_SEG_DEL[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ARMED) {
        dev->state.buttons[SP303_BTN_DEL].lit = dev->blink_on;
        if (dev->delete_target_pad >= 0) {
            int bank_start = dev->state.active_bank * 8;
            int local_pad = dev->delete_target_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[SP303_BTN_PAD_1 + local_pad].lit = true;
            }
        }
        sp303_display_raw(dev, SP303_SEG_DEL[0], SP303_SEG_DEL[1], SP303_SEG_DEL[2]);
    }
    else if (dev->sampling_state == SAMPLING_TRUNCATE_ARMED) {
        dev->state.buttons[SP303_BTN_DEL].lit = dev->blink_on;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[SP303_BTN_PAD_1 + i].lit = dev->blink_on;
            }
        }
        if (dev->last_played_pad >= 0) {
            int local_pad = dev->last_played_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[SP303_BTN_PAD_1 + local_pad].lit = true;
            }
        }
        sp303_display_raw(dev, SP303_SEG_TRC[0], SP303_SEG_TRC[1], SP303_SEG_TRC[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ALL_SELECT) {
        dev->state.buttons[SP303_BTN_DEL].lit = true;
        dev->state.buttons[SP303_BTN_BANK_A].lit = dev->blink_on;
        dev->state.buttons[SP303_BTN_BANK_B].lit = dev->blink_on;
        dev->state.buttons[SP303_BTN_BANK_C].lit = dev->blink_on;
        dev->state.buttons[SP303_BTN_BANK_D].lit = dev->blink_on;
        sp303_display_raw(dev, SP303_SEG_DAL[0], SP303_SEG_DAL[1], SP303_SEG_DAL[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_ALL_ARMED) {
        dev->state.buttons[SP303_BTN_DEL].lit = dev->blink_on;
        if (dev->delete_all_group == 0) {
            dev->state.buttons[SP303_BTN_BANK_A].lit = true;
            dev->state.buttons[SP303_BTN_BANK_B].lit = true;
        } else if (dev->delete_all_group == 1) {
            dev->state.buttons[SP303_BTN_BANK_C].lit = true;
            dev->state.buttons[SP303_BTN_BANK_D].lit = true;
        }
        sp303_display_raw(dev, SP303_SEG_DAL[0], SP303_SEG_DAL[1], SP303_SEG_DAL[2]);
    }

    if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
        dev->pad_has_sample[dev->last_played_pad]) {
        dev->state.buttons[SP303_BTN_LOOP].lit = dev->pad_loop_mode[dev->last_played_pad];
        dev->state.buttons[SP303_BTN_GATE].lit = dev->pad_gate_mode[dev->last_played_pad];
    }

    if (dev->sampling_stereo) {
        dev->state.buttons[SP303_BTN_STEREO].lit = true;
    }
    if (dev->sampling_quality == SP303_SAMPLE_QUALITY_LONG) {
        dev->state.buttons[SP303_BTN_LONG_LOFI].lit = true;
    } else if (dev->sampling_quality == SP303_SAMPLE_QUALITY_LOFI) {
        dev->state.buttons[SP303_BTN_LONG_LOFI].lit = dev->blink_on;
    }
}

// ─── Output ───────────────────────────────────────────────────────────────────

SP303State sp303_get_state(const SP303Device* dev) {
    SP303State out;
    std::memset(&out, 0, sizeof(out));
    if (dev) out = dev->state;
    return out;
}

// ─── Sampling control (called by renderer to sync with audio) ─────────────────

bool sp303_is_sampling_standby(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_STANDBY;
}

bool sp303_is_sampling_ready(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_READY;
}

bool sp303_is_recording(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_RECORDING;
}

bool sp303_is_threshold_mode(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_THRESHOLD;
}

bool sp303_is_start_end_level_mode(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_EDIT;
}

bool sp303_is_delete_mode(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_state == SAMPLING_DELETE_SELECT ||
           dev->sampling_state == SAMPLING_DELETE_ARMED ||
           dev->sampling_state == SAMPLING_TRUNCATE_ARMED ||
           dev->sampling_state == SAMPLING_DELETE_ALL_SELECT ||
           dev->sampling_state == SAMPLING_DELETE_ALL_ARMED ||
           dev->sampling_state == SAMPLING_SWAP_SELECT_A ||
           dev->sampling_state == SAMPLING_SWAP_SELECT_B;
}

int sp303_get_sampling_target_pad(const SP303Device* dev) {
    if (!dev) return -1;
    return dev->sampling_target_pad;
}

int sp303_get_last_sampling_target_pad(const SP303Device* dev) {
    if (!dev) return -1;
    return dev->last_sampling_target_pad;
}

int sp303_get_sample_level_threshold(const SP303Device* dev) {
    if (!dev) return 0;
    return dev->sample_level_threshold;
}

int sp303_get_last_played_pad(const SP303Device* dev) {
    if (!dev) return -1;
    return dev->last_played_pad;
}

int sp303_consume_record_bpm_quantize(SP303Device* dev) {
    if (!dev) return -1;
    int bpm = dev->pending_record_bpm_quantize;
    dev->pending_record_bpm_quantize = -1;
    return bpm;
}

bool sp303_get_pad_loop_mode(const SP303Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_loop_mode[pad_index];
}

bool sp303_get_pad_gate_mode(const SP303Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_gate_mode[pad_index];
}

void sp303_set_sampling_full(SP303Device* dev) {
    if (!dev) return;
    dev->sampling_full = true;
    // Auto-stop recording on FuL
    if (dev->sampling_state == SAMPLING_RECORDING) {
        dev->sampling_state = SAMPLING_IDLE;
        dev->sampling_target_pad = -1;
    }
}

void sp303_start_threshold_recording(SP303Device* dev) {
    if (!dev || dev->sampling_state != SAMPLING_READY || dev->sampling_target_pad < 0) return;
    dev->sampling_state = SAMPLING_RECORDING;
    dev->sampling_full = false;
}

void sp303_finish_threshold_recording(SP303Device* dev) {
    if (!dev || dev->sampling_state != SAMPLING_RECORDING) return;
    dev->sampling_state = SAMPLING_IDLE;
    if (dev->sampling_target_pad >= 0) {
        dev->pad_has_sample[dev->sampling_target_pad] = true;
        dev->pad_loop_mode[dev->sampling_target_pad] = false;
        dev->pad_gate_mode[dev->sampling_target_pad] = false; // trigger playback by default
        dev->last_sampling_target_pad = dev->sampling_target_pad;
        dev->last_played_pad = dev->sampling_target_pad;
        dev->pad_led_hold_frames[dev->sampling_target_pad] = 180;
        if (dev->bpm_armed_for_next_sample && dev->bpm_value >= 40 && dev->bpm_value <= 200) {
            dev->pending_record_bpm_quantize = dev->bpm_value;
            dev->bpm_armed_for_next_sample = false;
        }
    }
    dev->sampling_target_pad = -1;
    dev->sampling_full = false;
    sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
}

void sp303_note_pad_played(SP303Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->last_played_pad = pad_index;
    dev->pad_led_hold_frames[pad_index] = 180;
}

void sp303_set_edit_display_value(SP303Device* dev, int value) {
    if (!dev) return;
    int n = std::clamp(value, 0, 999);
    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;
    dev->state.display.digit[0] = (hundreds > 0) ? SP303_SEG_DIGITS[hundreds] : SP303_SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SP303_SEG_DIGITS[tens]     : SP303_SEG_BLANK;
    dev->state.display.digit[2] = SP303_SEG_DIGITS[ones];
    dev->edit_display_locked = true;
}

int sp303_consume_deleted_pad(SP303Device* dev) {
    if (!dev) return -1;
    int out = dev->deleted_pad_pending;
    dev->deleted_pad_pending = -1;
    return out;
}

int sp303_consume_truncate_pad(SP303Device* dev) {
    if (!dev) return -1;
    int out = dev->truncate_pad_pending;
    dev->truncate_pad_pending = -1;
    return out;
}

int sp303_consume_delete_all_group(SP303Device* dev) {
    if (!dev) return -1;
    int out = dev->delete_all_pending_group;
    dev->delete_all_pending_group = -1;
    return out;
}

bool sp303_consume_swap_pads(SP303Device* dev, int* pad_a, int* pad_b) {
    if (!dev || !pad_a || !pad_b) return false;
    if (dev->swap_pending_a < 0 || dev->swap_pending_b < 0) return false;
    *pad_a = dev->swap_pending_a;
    *pad_b = dev->swap_pending_b;
    dev->swap_pending_a = -1;
    dev->swap_pending_b = -1;
    return true;
}

void sp303_set_mark_lit(SP303Device* dev, bool lit) {
    if (!dev) return;
    dev->state.buttons[SP303_BTN_MARK].lit = lit;
}

bool sp303_get_sampling_stereo(const SP303Device* dev) {
    if (!dev) return false;
    return dev->sampling_stereo;
}

SP303SampleQuality sp303_get_sampling_quality(const SP303Device* dev) {
    if (!dev) return SP303_SAMPLE_QUALITY_STANDARD;
    return dev->sampling_quality;
}

// ─── Display helpers ──────────────────────────────────────────────────────────

void sp303_display_number(SP303Device* dev, int n) {
    if (!dev) return;
    n = std::clamp(n, 0, 999);

    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;

    // Add decimal point to each digit (1.2.3. format)
    dev->state.display.digit[0] = ((hundreds > 0) ? SP303_SEG_DIGITS[hundreds] : SP303_SEG_BLANK) | SP303_SEG_DP;
    dev->state.display.digit[1] = ((n >= 10)      ? SP303_SEG_DIGITS[tens]     : SP303_SEG_BLANK) | SP303_SEG_DP;
    dev->state.display.digit[2] = SP303_SEG_DIGITS[ones] | SP303_SEG_DP;
}

void sp303_display_raw(SP303Device* dev, uint8_t d0, uint8_t d1, uint8_t d2) {
    if (!dev) return;
    dev->state.display.digit[0] = d0;
    dev->state.display.digit[1] = d1;
    dev->state.display.digit[2] = d2;
}

// ─── Pad sample query ─────────────────────────────────────────────────────────

bool sp303_pad_has_sample(const SP303Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_has_sample[pad_index];
}

void sp303_pad_clear_sample(SP303Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_has_sample[pad_index] = false;
    dev->pad_loop_mode[pad_index] = false;
    dev->pad_gate_mode[pad_index] = false;
}
