#include "sp303.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace sp303 {

static constexpr uint32_t CORE_TAP_SAMPLE_RATE = 44100;

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

// ─── Sampling state machine ───────────────────────────────────────────────────

typedef enum {
    SAMPLING_IDLE,
    SAMPLING_STANDBY,
    SAMPLING_READY,
    SAMPLING_RECORDING,
    SAMPLING_THRESHOLD,
    SAMPLING_EDIT,
    SAMPLING_DELETE_SELECT,
    SAMPLING_DELETE_ARMED,
    SAMPLING_TRUNCATE_ARMED,
    SAMPLING_DELETE_ALL_SELECT,
    SAMPLING_DELETE_ALL_ARMED,
    SAMPLING_SWAP_SELECT_A,
    SAMPLING_SWAP_SELECT_B,
    SAMPLING_RESAMPLE_SOURCE,
    SAMPLING_RESAMPLE_DEST,
    SAMPLING_RESAMPLE_ARMED,
    SAMPLING_RESAMPLE_RECORDING,
} SamplingState;

// ─── Internal device state ────────────────────────────────────────────────────

struct Device {
    State state;

    uint32_t sample_clock;

    SamplingState sampling_state;
    int sampling_target_pad;
    int last_sampling_target_pad;
    bool sampling_full;
    int sample_level_threshold;
    int last_played_pad;
    int pad_led_hold_frames[32];
    bool edit_display_locked;
    int delete_target_pad;
    int deleted_pad_pending;
    int truncate_pad_pending;
    int delete_all_group;
    int delete_all_pending_group;
    int swap_pad_a;
    int swap_pad_b;
    int swap_pending_a;
    int swap_pending_b;
    bool sampling_stereo;
    SampleQuality sampling_quality;
    bool bpm_edit_mode;
    int bpm_value;
    bool bpm_armed_for_next_sample;
    bool time_bpm_mode;
    int time_bpm_pad;
    uint32_t tap_times[4];
    int tap_count;
    int pending_record_bpm_quantize;
    int rec_gain_display_frames;
    int rec_gain_value;
    int resample_source_pad;
    int resample_dest_pad;
    bool pad_loop_mode[32];
    bool pad_gate_mode[32];
    bool pad_reverse_mode[32];

    bool pad_has_sample[32];

    int  active_effect_btn;     // -1 or ButtonID of globally active effect
    bool pad_has_effect[32];    // true if pad carries the active effect
    int  mfx_type;              // 0-20: MFX sub-type, selected by holding MFX + CTRL 3

    uint32_t blink_phase;
    bool blink_on;
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
    dev->sampling_stereo = false;
    dev->sampling_quality = SAMPLE_QUALITY_STANDARD;
    dev->bpm_edit_mode = false;
    dev->bpm_value = -1;
    dev->bpm_armed_for_next_sample = false;
    dev->time_bpm_mode = false;
    dev->time_bpm_pad = -1;
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

    dev->active_effect_btn = -1;
    std::memset(dev->pad_has_effect, 0, sizeof(dev->pad_has_effect));
    dev->mfx_type = 0;

    dev->blink_phase = 0;
    dev->blink_on = false;

    return dev;
}

void destroy(Device* dev) {
    std::free(dev);
}

// ─── Helper: Get first empty pad in current bank ──────────────────────────────

static int find_empty_pad(Device* dev) {
    int bank_start = dev->state.active_bank * 8;
    for (int i = 0; i < 8; ++i) {
        int pad = bank_start + i;
        if (!dev->pad_has_sample[pad]) return pad;
    }
    return -1;
}

// ─── Helper: Get pad ID from bank + physical pad index ────────────────────────

static int get_pad_button_id(int pad_index) {
    return BTN_PAD_1 + pad_index;
}

static bool is_effect_btn(ButtonID btn) {
    return btn == BTN_FILTER_DRIVE || btn == BTN_PITCH || btn == BTN_DELAY ||
           btn == BTN_MFX || btn == BTN_VINYL_SIM || btn == BTN_ISOLATOR;
}

static void set_display_number_plain(Device* dev, int value) {
    int n = std::clamp(value, 0, 999);
    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;
    dev->state.display.digit[0] = (hundreds > 0) ? SEG_DIGITS[hundreds] : SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SEG_DIGITS[tens]     : SEG_BLANK;
    dev->state.display.digit[2] = SEG_DIGITS[ones];
}

// ─── Input ────────────────────────────────────────────────────────────────────

void button_down(Device* dev, ButtonID btn) {
    if (!dev || btn < 0 || btn >= BTN_COUNT) return;
    dev->state.buttons[btn].pressed = true;

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

    if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        if (btn == BTN_DEL) {
            dev->sampling_state = SAMPLING_IDLE;
            dev->delete_target_pad = -1;
            display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
            return;
        }
        if (btn == BTN_MARK) {
            if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
                dev->pad_has_sample[dev->last_played_pad]) {
                dev->sampling_state = SAMPLING_TRUNCATE_ARMED;
                display_raw(dev, SEG_TRC[0], SEG_TRC[1], SEG_TRC[2]);
            }
            return;
        }
        if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
            int physical_pad = btn - BTN_PAD_1;
            int actual_pad = dev->state.active_bank * 8 + physical_pad;
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
                dev->pad_has_sample[dev->last_played_pad]) {
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
        if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
            int physical_pad = btn - BTN_PAD_1;
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
                return;
            }
        } else if (dev->sampling_state == SAMPLING_RESAMPLE_DEST) {
            if (btn == BTN_REC && dev->resample_dest_pad >= 0) {
                dev->sampling_state = SAMPLING_RESAMPLE_ARMED;
                return;
            }
            if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
                int actual_pad = dev->state.active_bank * 8 + (btn - BTN_PAD_1);
                if (!dev->pad_has_sample[actual_pad]) {
                    dev->resample_dest_pad = actual_pad;
                    dev->sampling_target_pad = actual_pad;
                }
                return;
            }
        } else if (dev->sampling_state == SAMPLING_RESAMPLE_ARMED) {
            if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
                int actual_pad = dev->state.active_bank * 8 + (btn - BTN_PAD_1);
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
    }

    // Bank switch
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
                   dev->last_played_pad >= 0 &&
                   dev->last_played_pad < 32 &&
                   dev->pad_has_sample[dev->last_played_pad]) {
            dev->time_bpm_mode = !dev->time_bpm_mode;
            dev->time_bpm_pad = dev->time_bpm_mode ? dev->last_played_pad : -1;
        }
        return;
    }

    if (btn == BTN_TAP_TEMPO &&
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
        if (dev->sampling_state == SAMPLING_IDLE && dev->last_played_pad >= 0) {
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
        dev->sampling_stereo = !dev->sampling_stereo;
        return;
    }

    if (btn == BTN_LONG_LOFI) {
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
        dev->active_effect_btn = (int)btn;
        if (dev->state.buttons[BTN_REMAIN].pressed) {
            for (int i = 0; i < 32; ++i) dev->pad_has_effect[i] = true;
        } else if (dev->last_played_pad >= 0) {
            dev->pad_has_effect[dev->last_played_pad] = !dev->pad_has_effect[dev->last_played_pad];
        }
        return;
    }

    // ─── Pad press during REMAIN hold (toggle effect) ─────────────────────────
    if (dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_REMAIN].pressed &&
        btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
        int actual_pad = dev->state.active_bank * 8 + (btn - BTN_PAD_1);
        if (dev->active_effect_btn >= 0) {
            dev->pad_has_effect[actual_pad] = !dev->pad_has_effect[actual_pad];
        }
        return;
    }

    // ─── Pad selection during sampling standby ────────────────────────────────
    if (dev->sampling_state == SAMPLING_STANDBY) {
        if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
            int physical_pad = btn - BTN_PAD_1;
            int actual_pad = dev->state.active_bank * 8 + physical_pad;
            dev->sampling_target_pad = actual_pad;
            return;
        }
    }
}

void button_up(Device* dev, ButtonID btn) {
    if (!dev || btn < 0 || btn >= BTN_COUNT) return;
    dev->state.buttons[btn].pressed = false;
}

void knob_set(Device* dev, KnobID knob, float value) {
    if (!dev || knob < 0 || knob >= KNOB_COUNT) return;
    float clamped = std::clamp(value, 0.0f, 1.0f);
    dev->state.knobs[knob].value = clamped;

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
    if (knob == KNOB_RESONANCE &&
        dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_MFX].pressed) {
        dev->mfx_type = std::clamp((int)std::lround(clamped * 20.0f), 0, 20);
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

    for (int i = 0; i < 32; ++i) {
        if (dev->pad_led_hold_frames[i] > 0) dev->pad_led_hold_frames[i]--;
    }
    if (dev->rec_gain_display_frames > 0 &&
        dev->sampling_state != SAMPLING_RESAMPLE_SOURCE &&
        dev->sampling_state != SAMPLING_RESAMPLE_DEST &&
        dev->sampling_state != SAMPLING_RESAMPLE_ARMED) {
        dev->rec_gain_display_frames--;
    }

    // ─── Update LED states ────────────────────────────────────────────────────

    dev->state.buttons[BTN_REC].lit = false;
    dev->state.buttons[BTN_START_END_LEVEL].lit = false;
    dev->state.buttons[BTN_TIME_BPM].lit = false;
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

    if (dev->sampling_state == SAMPLING_IDLE) {
        dev->state.buttons[BTN_TIME_BPM].lit = dev->time_bpm_mode;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_led_hold_frames[bank_start + i] > 0) {
                dev->state.buttons[BTN_PAD_1 + i].lit = true;
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
                dev->state.buttons[BTN_PAD_1 + i].lit = (i == selected_local);
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
                    dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
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
                dev->state.buttons[BTN_PAD_1 + local_pad].lit = true;
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
                dev->state.buttons[BTN_PAD_1 + local_pad].lit = true;
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
                dev->state.buttons[BTN_PAD_1 + local_pad].lit = true;
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                int pad = bank_start + i;
                if (!dev->pad_has_sample[pad]) {
                    dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
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
                dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
            }
        }
        if (local_dst >= 0 && local_dst < 8) dev->state.buttons[BTN_PAD_1 + local_dst].lit = true;
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
            dev->state.buttons[BTN_PAD_1 + local_dst].lit = true;
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
            if (dev->pad_led_hold_frames[bank_start + i] > 0) {
                dev->state.buttons[BTN_PAD_1 + i].lit = true;
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
                dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
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
            if (local_a >= 0 && local_a < 8) dev->state.buttons[BTN_PAD_1 + local_a].lit = true;
            if (local_b >= 0 && local_b < 8) dev->state.buttons[BTN_PAD_1 + local_b].lit = true;
        } else {
            dev->state.buttons[BTN_REC].lit = true;
            for (int i = 0; i < 8; ++i) {
                if (dev->pad_has_sample[bank_start + i]) {
                    dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
                }
            }
            if (local_a >= 0 && local_a < 8) dev->state.buttons[BTN_PAD_1 + local_a].lit = true;
        }
        display_raw(dev, SEG_CHG[0], SEG_CHG[1], SEG_CHG[2]);
    }
    else if (dev->sampling_state == SAMPLING_DELETE_SELECT) {
        dev->state.buttons[BTN_DEL].lit = true;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
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
                dev->state.buttons[BTN_PAD_1 + local_pad].lit = true;
            }
        }
        display_raw(dev, SEG_DEL[0], SEG_DEL[1], SEG_DEL[2]);
    }
    else if (dev->sampling_state == SAMPLING_TRUNCATE_ARMED) {
        dev->state.buttons[BTN_DEL].lit = dev->blink_on;
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            if (dev->pad_has_sample[bank_start + i]) {
                dev->state.buttons[BTN_PAD_1 + i].lit = dev->blink_on;
            }
        }
        if (dev->last_played_pad >= 0) {
            int local_pad = dev->last_played_pad - bank_start;
            if (local_pad >= 0 && local_pad < 8) {
                dev->state.buttons[BTN_PAD_1 + local_pad].lit = true;
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

    if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
        dev->pad_has_sample[dev->last_played_pad]) {
        dev->state.buttons[BTN_LOOP].lit = dev->pad_loop_mode[dev->last_played_pad];
        dev->state.buttons[BTN_GATE].lit = dev->pad_gate_mode[dev->last_played_pad];
        dev->state.buttons[BTN_REVERSE].lit = dev->pad_reverse_mode[dev->last_played_pad];
    }

    if (dev->sampling_stereo) {
        dev->state.buttons[BTN_STEREO].lit = true;
    }
    if (dev->sampling_quality == SAMPLE_QUALITY_LONG) {
        dev->state.buttons[BTN_LONG_LOFI].lit = true;
    } else if (dev->sampling_quality == SAMPLE_QUALITY_LOFI) {
        dev->state.buttons[BTN_LONG_LOFI].lit = dev->blink_on;
    }

    // ─── Effect button LED — lit when selected pad carries the active effect ───
    if (dev->last_played_pad >= 0 && dev->last_played_pad < 32 &&
        dev->pad_has_effect[dev->last_played_pad] && dev->active_effect_btn >= 0) {
        dev->state.buttons[dev->active_effect_btn].lit = true;
    }

    // ─── REMAIN hold — pad LEDs show which pads carry the active effect ───────
    if (dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_REMAIN].pressed &&
        dev->active_effect_btn >= 0) {
        int bank_start = dev->state.active_bank * 8;
        for (int i = 0; i < 8; ++i) {
            dev->state.buttons[BTN_PAD_1 + i].lit = dev->pad_has_effect[bank_start + i];
        }
    }

    // ─── MFX held — show sub-type number (1-21) on display ───────────────────
    if (dev->sampling_state == SAMPLING_IDLE &&
        dev->state.buttons[BTN_MFX].pressed) {
        set_display_number_plain(dev, dev->mfx_type + 1);
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

int consume_record_bpm_quantize(Device* dev) {
    if (!dev) return -1;
    int bpm = dev->pending_record_bpm_quantize;
    dev->pending_record_bpm_quantize = -1;
    return bpm;
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

void note_pad_played(Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->last_played_pad = pad_index;
    dev->pad_led_hold_frames[pad_index] = 180;
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

void set_mark_lit(Device* dev, bool lit) {
    if (!dev) return;
    dev->state.buttons[BTN_MARK].lit = lit;
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

// ─── Pad sample query ─────────────────────────────────────────────────────────

bool pad_has_sample(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_has_sample[pad_index];
}

void pad_clear_sample(Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_has_sample[pad_index] = false;
    dev->pad_loop_mode[pad_index] = false;
    dev->pad_gate_mode[pad_index] = false;
    dev->pad_reverse_mode[pad_index] = false;
    dev->pad_has_effect[pad_index] = false;
}

bool get_pad_project_state(const Device* dev, int pad_index, PadProjectState* out) {
    if (!dev || !out || pad_index < 0 || pad_index >= 32) return false;
    out->has_sample = dev->pad_has_sample[pad_index];
    out->loop_mode = dev->pad_loop_mode[pad_index];
    out->gate_mode = dev->pad_gate_mode[pad_index];
    out->reverse_mode = dev->pad_reverse_mode[pad_index];
    out->has_effect = dev->pad_has_effect[pad_index];
    out->bpm_adjust = 0;
    out->time_mode = 0;
    out->time_target_bpm = -1;
    return true;
}

void set_pad_project_state(Device* dev, int pad_index, const PadProjectState& state) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_has_sample[pad_index] = state.has_sample;
    dev->pad_loop_mode[pad_index] = state.loop_mode;
    dev->pad_gate_mode[pad_index] = state.gate_mode;
    dev->pad_reverse_mode[pad_index] = state.reverse_mode;
    dev->pad_has_effect[pad_index] = state.has_effect;
}

void reset_project_state(Device* dev) {
    if (!dev) return;
    for (int i = 0; i < 32; ++i) {
        pad_clear_sample(dev, i);
        dev->pad_led_hold_frames[i] = 0;
    }
    dev->sampling_state = SAMPLING_IDLE;
    dev->sampling_target_pad = -1;
    dev->last_sampling_target_pad = -1;
    dev->sampling_full = false;
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
    dev->bpm_edit_mode = false;
    dev->bpm_value = -1;
    dev->bpm_armed_for_next_sample = false;
    dev->time_bpm_mode = false;
    dev->time_bpm_pad = -1;
    dev->tap_count = 0;
    dev->pending_record_bpm_quantize = -1;
    dev->rec_gain_display_frames = 0;
    dev->rec_gain_value = 0;
    dev->active_effect_btn = -1;
    dev->state.active_bank = 0;
    display_raw(dev, SEG_DASH, SEG_DASH, SEG_DASH);
}

void set_sample_level_threshold(Device* dev, int level) {
    if (!dev) return;
    dev->sample_level_threshold = std::clamp(level, 0, 8);
}

void set_time_bpm_display_number(Device* dev, int value) {
    set_display_number_plain(dev, value);
}

void set_time_bpm_display_off(Device* dev) {
    if (!dev) return;
    display_raw(dev, SEG_OFF[0], SEG_OFF[1], SEG_OFF[2]);
}

void set_time_bpm_display_pattern(Device* dev) {
    if (!dev) return;
    display_raw(dev, SEG_PTN[0], SEG_PTN[1], SEG_PTN[2]);
}

int get_active_effect_btn(const Device* dev) {
    if (!dev) return -1;
    return dev->active_effect_btn;
}

bool get_pad_has_effect(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_has_effect[pad_index];
}

void set_active_effect_btn(Device* dev, int btn_id) {
    if (!dev) return;
    if (btn_id < 0 || btn_id >= BTN_COUNT) {
        dev->active_effect_btn = -1;
        return;
    }
    dev->active_effect_btn = btn_id;
}

int get_mfx_type(const Device* dev) {
    if (!dev) return 0;
    return dev->mfx_type;
}

} // namespace sp303
