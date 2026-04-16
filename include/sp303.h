#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Button IDs ───────────────────────────────────────────────────────────────

typedef enum {
    // DSP effect buttons
    SP303_BTN_FILTER_DRIVE = 0,
    SP303_BTN_PITCH,
    SP303_BTN_DELAY,
    SP303_BTN_MFX,
    SP303_BTN_VINYL_SIM,
    SP303_BTN_ISOLATOR,

    // Sample edit (mode selectors for CTRL knobs)
    SP303_BTN_START_END_LEVEL,
    SP303_BTN_TIME_BPM,

    // Pattern sequencer
    SP303_BTN_PATTERN_SELECT,
    SP303_BTN_LENGTH,
    SP303_BTN_QUANTIZE,

    // Transport / utility
    SP303_BTN_TAP_TEMPO,      // alt1: EFFECT GRAB
    SP303_BTN_CANCEL,         // alt1: PATTERN STOP
    SP303_BTN_REMAIN,         // alt1: CURRENT PAD

    // Sample mode
    SP303_BTN_LONG_LOFI,      // primary: LONG,  alt1: LOFI
    SP303_BTN_STEREO,
    SP303_BTN_GATE,
    SP303_BTN_LOOP,
    SP303_BTN_REVERSE,

    // Recording / editing
    SP303_BTN_DEL,
    SP303_BTN_REC,            // alt1: PATTERN REC
    SP303_BTN_RESAMPLE,
    SP303_BTN_MARK,           // alt1: START POINT,  alt2: END POINT

    // Bank select
    SP303_BTN_BANK_A,
    SP303_BTN_BANK_B,
    SP303_BTN_BANK_C,
    SP303_BTN_BANK_D,

    // Pads — 4 banks × 8 = 32 slots.
    // Bank A: PAD_1..PAD_8, Bank B: PAD_9..PAD_16, C: PAD_17..PAD_24, D: PAD_25..PAD_32.
    // Only 8 are visible at a time (determined by active_bank).
    SP303_BTN_PAD_1,
    SP303_BTN_PAD_2,
    SP303_BTN_PAD_3,
    SP303_BTN_PAD_4,
    SP303_BTN_PAD_5,
    SP303_BTN_PAD_6,
    SP303_BTN_PAD_7,
    SP303_BTN_PAD_8,
    SP303_BTN_PAD_9,
    SP303_BTN_PAD_10,
    SP303_BTN_PAD_11,
    SP303_BTN_PAD_12,
    SP303_BTN_PAD_13,
    SP303_BTN_PAD_14,
    SP303_BTN_PAD_15,
    SP303_BTN_PAD_16,
    SP303_BTN_PAD_17,
    SP303_BTN_PAD_18,
    SP303_BTN_PAD_19,
    SP303_BTN_PAD_20,
    SP303_BTN_PAD_21,
    SP303_BTN_PAD_22,
    SP303_BTN_PAD_23,
    SP303_BTN_PAD_24,
    SP303_BTN_PAD_25,
    SP303_BTN_PAD_26,
    SP303_BTN_PAD_27,
    SP303_BTN_PAD_28,
    SP303_BTN_PAD_29,
    SP303_BTN_PAD_30,
    SP303_BTN_PAD_31,
    SP303_BTN_PAD_32,

    // Special
    SP303_BTN_HOLD,
    SP303_BTN_EXT_SOURCE,

    SP303_BTN_COUNT
} SP303ButtonID;

// ─── Knob IDs ─────────────────────────────────────────────────────────────────

typedef enum {
    SP303_KNOB_VOLUME = 0,
    SP303_KNOB_CUTOFF,        // alt1: TIME,  alt2: START
    SP303_KNOB_RESONANCE,     // alt1: BPM,   alt2: END
    SP303_KNOB_DRIVE,         // alt1: LEVEL
    SP303_KNOB_COUNT
} SP303KnobID;

// ─── Metadata tables ──────────────────────────────────────────────────────────
//
// Indexed by SP303ButtonID / SP303KnobID. Useful for renderers that need to
// draw labels without hard-coding strings.

typedef struct {
    SP303ButtonID id;
    const char*   primary;
    const char*   alt1;   // NULL if none
    const char*   alt2;   // NULL if none
} SP303ButtonDef;

typedef struct {
    SP303KnobID id;
    const char* primary;
    const char* alt1;     // NULL if none
    const char* alt2;     // NULL if none
} SP303KnobDef;

extern const SP303ButtonDef SP303_BUTTON_DEFS[SP303_BTN_COUNT];
extern const SP303KnobDef   SP303_KNOB_DEFS[SP303_KNOB_COUNT];

// ─── 7-Segment display ────────────────────────────────────────────────────────
//
//      a
//     ---
//  f |   | b
//     -g-
//  e |   | c
//     ---
//      d    . (dp)
//
// Each digit is a bitmask: bits 0-6 = segments a-g, bit 7 = decimal point.

typedef enum {
    SP303_SEG_A  = 0x01,
    SP303_SEG_B  = 0x02,
    SP303_SEG_C  = 0x04,
    SP303_SEG_D  = 0x08,
    SP303_SEG_E  = 0x10,
    SP303_SEG_F  = 0x20,
    SP303_SEG_G  = 0x40,
    SP303_SEG_DP = 0x80,
} SP303Segment;

extern const uint8_t SP303_SEG_DIGITS[10]; // 0–9 pre-encoded
extern const uint8_t SP303_SEG_BLANK;      // all segments off
extern const uint8_t SP303_SEG_DASH;       // middle bar only  ( - )
extern const uint8_t SP303_SEG_ERR[3];     // "Err" across all 3 digits

typedef struct {
    uint8_t digit[3]; // [0] = leftmost, [2] = rightmost
} SP303Display;

// ─── Indicator IDs ───────────────────────────────────────────────────────────
//
// Read-only LEDs — no press state, driven entirely by internal logic.

typedef enum {
    SP303_IND_PEAK = 0,   // lights when output level exceeds threshold
    SP303_IND_COUNT
} SP303IndicatorID;

// ─── Per-element state ────────────────────────────────────────────────────────

typedef struct {
    bool pressed;
    bool lit;
} SP303ButtonState;

typedef struct {
    float value; // 0.0 – 1.0
} SP303KnobState;

typedef struct {
    bool lit;
} SP303IndicatorState;

// ─── Full output snapshot ─────────────────────────────────────────────────────
//
// Everything a renderer or host needs to draw the device and query its state.
// Returned by value from sp303_get_state() — safe to copy across threads.

typedef struct {
    SP303ButtonState    buttons[SP303_BTN_COUNT];
    SP303KnobState      knobs[SP303_KNOB_COUNT];
    SP303IndicatorState indicators[SP303_IND_COUNT];
    SP303Display        display;
    uint8_t             active_bank; // 0=A, 1=B, 2=C, 3=D
} SP303State;

// ─── Device handle ────────────────────────────────────────────────────────────

typedef struct SP303Device SP303Device;

SP303Device*  sp303_create(void);
void          sp303_destroy(SP303Device* dev);

// Input — call from UI / MIDI thread
void          sp303_button_down   (SP303Device* dev, SP303ButtonID btn);
void          sp303_button_up     (SP303Device* dev, SP303ButtonID btn);
void          sp303_knob_set      (SP303Device* dev, SP303KnobID knob, float value);

// Indicator — call from audio engine (or any thread that detects the condition)
void          sp303_indicator_set (SP303Device* dev, SP303IndicatorID ind, bool lit);

// Clock — call from audio thread (or a timer at a fixed rate)
// samples_elapsed: number of audio samples since the last tick
void          sp303_tick(SP303Device* dev, uint32_t samples_elapsed);

// Output — returns a value copy; no locking needed by the caller
SP303State    sp303_get_state(const SP303Device* dev);

// Display helpers
void          sp303_display_number(SP303Device* dev, int n);                        // 0–999, left-pads with blanks
void          sp303_display_raw   (SP303Device* dev, uint8_t d0, uint8_t d1, uint8_t d2);

#ifdef __cplusplus
}
#endif
