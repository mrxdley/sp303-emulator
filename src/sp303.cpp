#include "sp303.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

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

// ─── Internal device state ────────────────────────────────────────────────────

struct SP303Device {
    SP303State state;

    // Internal clock accumulator (samples)
    uint32_t sample_clock;
};

// ─── Lifecycle ────────────────────────────────────────────────────────────────

SP303Device* sp303_create(void) {
    SP303Device* dev = static_cast<SP303Device*>(std::calloc(1, sizeof(SP303Device)));
    if (!dev) return nullptr;

    // All buttons off, all knobs at zero
    std::memset(&dev->state, 0, sizeof(SP303State));
    dev->sample_clock = 0;

    // Power-on: blank display
    dev->state.display.digit[0] = SP303_SEG_BLANK;
    dev->state.display.digit[1] = SP303_SEG_BLANK;
    dev->state.display.digit[2] = SP303_SEG_BLANK;

    // Default bank: A
    dev->state.active_bank = 0;
    dev->state.buttons[SP303_BTN_BANK_A].lit = true;

    return dev;
}

void sp303_destroy(SP303Device* dev) {
    std::free(dev);
}

// ─── Input ────────────────────────────────────────────────────────────────────

void sp303_button_down(SP303Device* dev, SP303ButtonID btn) {
    if (!dev || btn < 0 || btn >= SP303_BTN_COUNT) return;
    dev->state.buttons[btn].pressed = true;

    // Bank switch: light the pressed bank, unlight the others, update active_bank
    if (btn >= SP303_BTN_BANK_A && btn <= SP303_BTN_BANK_D) {
        uint8_t bank = static_cast<uint8_t>(btn - SP303_BTN_BANK_A);
        dev->state.active_bank = bank;
        for (int b = 0; b < 4; ++b)
            dev->state.buttons[SP303_BTN_BANK_A + b].lit = (b == bank);
    }
}

void sp303_button_up(SP303Device* dev, SP303ButtonID btn) {
    if (!dev || btn < 0 || btn >= SP303_BTN_COUNT) return;
    dev->state.buttons[btn].pressed = false;
}

void sp303_knob_set(SP303Device* dev, SP303KnobID knob, float value) {
    if (!dev || knob < 0 || knob >= SP303_KNOB_COUNT) return;
    dev->state.knobs[knob].value = std::clamp(value, 0.0f, 1.0f);
}

void sp303_indicator_set(SP303Device* dev, SP303IndicatorID ind, bool lit) {
    if (!dev || ind < 0 || ind >= SP303_IND_COUNT) return;
    dev->state.indicators[ind].lit = lit;
}

// ─── Clock ────────────────────────────────────────────────────────────────────

void sp303_tick(SP303Device* dev, uint32_t samples_elapsed) {
    if (!dev) return;
    dev->sample_clock += samples_elapsed;
    // Sequencer, LED blink, display update logic will go here.
}

// ─── Output ───────────────────────────────────────────────────────────────────

SP303State sp303_get_state(const SP303Device* dev) {
    SP303State out;
    std::memset(&out, 0, sizeof(out));
    if (dev) out = dev->state;
    return out;
}

// ─── Display helpers ──────────────────────────────────────────────────────────

void sp303_display_number(SP303Device* dev, int n) {
    if (!dev) return;
    n = std::clamp(n, 0, 999);

    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;

    dev->state.display.digit[0] = (hundreds > 0) ? SP303_SEG_DIGITS[hundreds] : SP303_SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SP303_SEG_DIGITS[tens]     : SP303_SEG_BLANK;
    dev->state.display.digit[2] = SP303_SEG_DIGITS[ones];
}

void sp303_display_raw(SP303Device* dev, uint8_t d0, uint8_t d1, uint8_t d2) {
    if (!dev) return;
    dev->state.display.digit[0] = d0;
    dev->state.display.digit[1] = d1;
    dev->state.display.digit[2] = d2;
}
