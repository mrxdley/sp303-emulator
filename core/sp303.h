#pragma once
#include <stdint.h>
#include <vector>

namespace sp303 {

// ─── Button IDs ───────────────────────────────────────────────────────────────

enum ButtonID {
    // DSP effect buttons
    BTN_FILTER_DRIVE = 0,
    BTN_PITCH,
    BTN_DELAY,
    BTN_MFX,
    BTN_VINYL_SIM,
    BTN_ISOLATOR,

    // Sample edit (mode selectors for CTRL knobs)
    BTN_START_END_LEVEL,
    BTN_TIME_BPM,

    // Pattern sequencer
    BTN_PATTERN_SELECT,
    BTN_LENGTH,
    BTN_QUANTIZE,

    // Transport / utility
    BTN_TAP_TEMPO,      // alt1: EFFECT GRAB
    BTN_CANCEL,         // alt1: PATTERN STOP
    BTN_REMAIN,         // alt1: CURRENT PAD

    // Sample mode
    BTN_LONG_LOFI,      // primary: LONG,  alt1: LOFI
    BTN_STEREO,
    BTN_GATE,
    BTN_LOOP,
    BTN_REVERSE,

    // Recording / editing
    BTN_DEL,
    BTN_REC,            // alt1: PATTERN REC
    BTN_RESAMPLE,
    BTN_MARK,           // alt1: START POINT,  alt2: END POINT

    // Bank select
    BTN_BANK_A,
    BTN_BANK_B,
    BTN_BANK_C,
    BTN_BANK_D,

    // Pads — 4 banks × 8 = 32 slots.
    // Bank A: PAD_1..PAD_8, Bank B: PAD_9..PAD_16, C: PAD_17..PAD_24, D: PAD_25..PAD_32.
    // Only 8 are visible at a time (determined by active_bank).
    BTN_PAD_1,
    BTN_PAD_2,
    BTN_PAD_3,
    BTN_PAD_4,
    BTN_PAD_5,
    BTN_PAD_6,
    BTN_PAD_7,
    BTN_PAD_8,
    BTN_PAD_9,
    BTN_PAD_10,
    BTN_PAD_11,
    BTN_PAD_12,
    BTN_PAD_13,
    BTN_PAD_14,
    BTN_PAD_15,
    BTN_PAD_16,
    BTN_PAD_17,
    BTN_PAD_18,
    BTN_PAD_19,
    BTN_PAD_20,
    BTN_PAD_21,
    BTN_PAD_22,
    BTN_PAD_23,
    BTN_PAD_24,
    BTN_PAD_25,
    BTN_PAD_26,
    BTN_PAD_27,
    BTN_PAD_28,
    BTN_PAD_29,
    BTN_PAD_30,
    BTN_PAD_31,
    BTN_PAD_32,

    // Special
    BTN_HOLD,
    BTN_EXT_SOURCE,

    BTN_COUNT
};

// ─── Knob IDs ─────────────────────────────────────────────────────────────────

enum KnobID {
    KNOB_VOLUME = 0,
    KNOB_CUTOFF,        // alt1: TIME,  alt2: START
    KNOB_RESONANCE,     // alt1: BPM,   alt2: END
    KNOB_DRIVE,         // alt1: LEVEL
    KNOB_COUNT
};

enum SampleQuality {
    SAMPLE_QUALITY_STANDARD = 0,
    SAMPLE_QUALITY_LONG,
    SAMPLE_QUALITY_LOFI,
};

// ─── Metadata tables ──────────────────────────────────────────────────────────
//
// Indexed by ButtonID / KnobID. Useful for renderers that need to draw labels
// without hard-coding strings.

struct ButtonDef {
    ButtonID    id;
    const char* primary;
    const char* alt1;   // nullptr if none
    const char* alt2;   // nullptr if none
};

struct KnobDef {
    KnobID      id;
    const char* primary;
    const char* alt1;   // nullptr if none
    const char* alt2;   // nullptr if none
};

extern const ButtonDef BUTTON_DEFS[BTN_COUNT];
extern const KnobDef   KNOB_DEFS[KNOB_COUNT];

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

enum Segment {
    SEG_A  = 0x01,
    SEG_B  = 0x02,
    SEG_C  = 0x04,
    SEG_D  = 0x08,
    SEG_E  = 0x10,
    SEG_F  = 0x20,
    SEG_G  = 0x40,
    SEG_DP = 0x80,
};

extern const uint8_t SEG_DIGITS[10]; // 0–9 pre-encoded
extern const uint8_t SEG_BLANK;      // all segments off
extern const uint8_t SEG_DASH;       // middle bar only  ( - )
extern const uint8_t SEG_ERR[3];     // "Err" across all 3 digits
extern const uint8_t SEG_FUL[3];     // "FuL" across all 3 digits
extern const uint8_t SEG_REC[3];     // "rEC" across all 3 digits
extern const uint8_t SEG_RDY[3];     // "rdY" across all 3 digits
extern const uint8_t SEG_EDT[3];     // "Edt" across all 3 digits
extern const uint8_t SEG_DEL[3];     // "dEL" across all 3 digits
extern const uint8_t SEG_TRC[3];     // "trC" across all 3 digits
extern const uint8_t SEG_DAL[3];     // "dAL" across all 3 digits
extern const uint8_t SEG_CHG[3];     // "CHG" across all 3 digits
extern const uint8_t SEG_LEV[3];     // "LEV" across all 3 digits
extern const uint8_t SEG_ERS[3];     // "ErS" across all 3 digits
extern const uint8_t SEG_FMT[3];     // "FMt" across all 3 digits
extern const uint8_t SEG_EMP[3];     // "EMP" across all 3 digits
extern const uint8_t SEG_PRT[3];     // "Prt" across all 3 digits
extern const uint8_t SEG_PIT[3];     // "Pit" across all 3 digits
extern const uint8_t SEG_FDB[3];     // "Fdb" across all 3 digits
extern const uint8_t SEG_OFF[3];     // "oFF" across all 3 digits
extern const uint8_t SEG_PTN[3];     // "Ptn" across all 3 digits
extern const uint8_t SEG_COT[3];     // "CoT" across all 3 digits
extern const uint8_t SEG_DRV[3];     // "dRV" across all 3 digits
extern const uint8_t SEG_LO[3];      // "Lo " across all 3 digits
extern const uint8_t SEG_MID[3];     // "Mid" across all 3 digits
extern const uint8_t SEG_HI[3];      // "Hi " across all 3 digits

struct Display {
    uint8_t digit[3]; // [0] = leftmost, [2] = rightmost
};

// ─── Indicator IDs ───────────────────────────────────────────────────────────
//
// Read-only LEDs — no press state, driven entirely by internal logic.

enum IndicatorID {
    IND_PEAK = 0,   // lights when output level exceeds threshold
    IND_COUNT
};

// ─── Per-element state ────────────────────────────────────────────────────────

struct ButtonState {
    bool pressed;
    bool lit;
};

struct KnobState {
    float value; // 0.0 – 1.0
};

struct IndicatorState {
    bool lit;
};

// ─── Full output snapshot ─────────────────────────────────────────────────────
//
// Everything a renderer or host needs to draw the device and query its state.
// Returned by value from get_state() — safe to copy across threads.

struct State {
    ButtonState    buttons[BTN_COUNT];
    KnobState      knobs[KNOB_COUNT];
    IndicatorState indicators[IND_COUNT];
    Display        display;
    uint8_t        active_bank; // 0=A, 1=B, 2=C, 3=D
};

struct PadProjectState {
    bool    has_sample = false;
    bool    loop_mode = false;
    bool    gate_mode = false;
    bool    reverse_mode = false;
    bool    has_effect = false;
    bool    recorded_stereo = false;
    int     recorded_quality = SAMPLE_QUALITY_STANDARD;
    int     bpm_adjust = 0;
    int     time_mode = 0;
    int     time_target_bpm = -1;
};

struct PatternProjectEvent {
    int type = 0;
    int tick = 0;
    int sample_pad = 0;
    int velocity = 127;
};

struct PatternProjectSlot {
    bool assigned = false;
    int length_measures = 1;
    int quantize = 4;
    int metronome_level = 100;
};

enum MemoryCardBackupKind {
    MEMORY_CARD_BACKUP_EMPTY = 0,
    MEMORY_CARD_BACKUP_SAMPLES,
    MEMORY_CARD_BACKUP_PATTERNS,
};

// ─── Device handle ────────────────────────────────────────────────────────────

struct Device;

Device* create();
void    destroy(Device* dev);

// Input — call from UI / MIDI thread
void button_down   (Device* dev, ButtonID btn);
void button_up     (Device* dev, ButtonID btn);
void knob_set      (Device* dev, KnobID knob, float value);

// Indicator — call from audio engine (or any thread that detects the condition)
void indicator_set (Device* dev, IndicatorID ind, bool lit);

// Clock — call from audio thread (or a timer at a fixed rate)
// samples_elapsed: number of audio samples since the last tick
void tick(Device* dev, uint32_t samples_elapsed);

// Output — returns a value copy; no locking needed by the caller
State get_state(const Device* dev);

// Display helpers
void display_number (Device* dev, int n);                       // 0–999, left-pads with blanks
void display_raw    (Device* dev, uint8_t d0, uint8_t d1, uint8_t d2);
uint8_t char_to_seg (char c);                                   // single char → segment byte
void display_str3   (Device* dev, const char* label3);          // 3-char string → display

// ─── Sampling control (renderer <-> core sync) ────────────────────────────────
// These allow the renderer to query sampling state and control the audio engine.

bool is_sampling_standby   (const Device* dev);
bool is_sampling_ready     (const Device* dev);
bool is_recording          (const Device* dev);
bool is_pattern_mode       (const Device* dev);
bool is_pattern_playing    (const Device* dev);
bool is_pattern_recording  (const Device* dev);
bool is_pattern_record_select(const Device* dev);
bool is_pattern_erase_mode (const Device* dev);
bool is_threshold_mode     (const Device* dev);
bool is_start_end_level_mode(const Device* dev);
bool is_time_bpm_mode      (const Device* dev);
bool is_delete_mode        (const Device* dev);
bool is_resampling_mode    (const Device* dev);
bool is_resample_source_select(const Device* dev);
bool is_resample_dest_select(const Device* dev);
bool is_resample_armed     (const Device* dev);
bool is_resample_recording (const Device* dev);
int  get_sampling_target_pad(const Device* dev);  // -1 if none selected
int  get_resample_source_pad(const Device* dev);   // -1 if none
int  get_resample_dest_pad  (const Device* dev);   // -1 if none
void set_sampling_full     (Device* dev);         // called when audio buffer fills
void start_threshold_recording (Device* dev);
void finish_threshold_recording(Device* dev);
void note_pad_played       (Device* dev, int pad_index, int velocity = 127);
void note_hold_toggled     (Device* dev);
void set_edit_display_value(Device* dev, int value);
int  consume_deleted_pad   (Device* dev); // -1 if none
int  consume_truncate_pad  (Device* dev); // -1 if none
int  consume_delete_all_group(Device* dev); // -1 none, 0=A/B, 1=C/D
bool consume_swap_pads     (Device* dev, int* pad_a, int* pad_b); // true if pending
bool consume_memory_card_dirty(Device* dev);
bool consume_memory_card_format(Device* dev);
int  consume_memory_card_sample_save(Device* dev); // backup slot 0-7, else -1
int  consume_memory_card_sample_load(Device* dev); // backup slot 0-7, else -1
void set_mark_lit          (Device* dev, bool lit);
void set_pad_playing       (Device* dev, int pad_index, bool playing);
void set_pad_marked        (Device* dev, int pad_index, bool marked);
bool get_sampling_stereo   (const Device* dev);
SampleQuality get_sampling_quality(const Device* dev);

// Last recorded pad (persists after recording stops, for audio assignment)
int  get_last_sampling_target_pad(const Device* dev);  // -1 if never recorded
int  get_sample_level_threshold  (const Device* dev);  // 0-8
int  get_last_played_pad         (const Device* dev);  // -1 if none
int  get_time_bpm_pad            (const Device* dev);  // -1 if none
int  get_pattern_bpm             (const Device* dev);  // 40-200
void set_pattern_bpm             (Device* dev, int bpm);
int  get_pattern_record_slot     (const Device* dev);  // -1 if none
int  get_pattern_metronome_level (const Device* dev);  // 0-127
int  consume_record_bpm_quantize (Device* dev);        // -1 if none, else 40-200
int  consume_mark_action         (Device* dev);        // 0 none, 1 start, 2 end, 3 reset full
int  get_mark_edit_pad           (const Device* dev);  // -1 if none
bool consume_pattern_trigger     (Device* dev, PatternProjectEvent* out); // false if none
int  consume_pattern_metronome   (Device* dev);        // 0 none, 1 weak, 2 strong
bool get_pad_loop_mode           (const Device* dev, int pad_index); // false=one-shot
bool get_pad_gate_mode           (const Device* dev, int pad_index); // false=trigger
bool get_pad_reverse_mode        (const Device* dev, int pad_index); // false=forward
void note_pattern_pad_played     (Device* dev, int pad_index, int velocity = 127);
bool get_pattern_project_slot    (const Device* dev, int slot, PatternProjectSlot* out);
bool set_pattern_project_slot    (Device* dev, int slot, const PatternProjectSlot& state);
bool get_pattern_project_events  (const Device* dev, int slot, PatternProjectEvent* out_events, int max_events, int* out_count);
bool set_pattern_project_events  (Device* dev, int slot, const PatternProjectEvent* events, int count);
bool get_memory_card_formatted   (const Device* dev);
void set_memory_card_formatted   (Device* dev, bool formatted);
bool get_memory_card_write_protected(const Device* dev);
void set_memory_card_write_protected(Device* dev, bool write_protected);
int  get_memory_card_backup_kind (const Device* dev, int backup_slot);
void set_memory_card_backup_kind (Device* dev, int backup_slot, int kind);
int  get_memory_card_backup_pattern_bpm(const Device* dev, int backup_slot);
void set_memory_card_backup_pattern_bpm(Device* dev, int backup_slot, int bpm);
bool get_memory_card_backup_sample_state(const Device* dev, int backup_slot, int index, PadProjectState* out);
bool set_memory_card_backup_sample_state(Device* dev, int backup_slot, int index, const PadProjectState& state);
bool get_memory_card_backup_sample_audio(const Device* dev, int backup_slot, int index, std::vector<float>* pcm, uint32_t* channels, uint32_t* frames);
bool set_memory_card_backup_sample_audio(Device* dev, int backup_slot, int index, const float* pcm, uint32_t frames, uint32_t channels);
bool get_memory_card_backup_sample_edit(const Device* dev, int backup_slot, int index,
                                        int* start_127, int* end_127, int* level_127,
                                        int* bpm_adjust, int* time_mode, int* time_target_bpm);
bool set_memory_card_backup_sample_edit(Device* dev, int backup_slot, int index,
                                        int start_127, int end_127, int level_127,
                                        int bpm_adjust, int time_mode, int time_target_bpm);
bool get_memory_card_backup_pattern_slot(const Device* dev, int backup_slot, int index, PatternProjectSlot* out);
bool set_memory_card_backup_pattern_slot(Device* dev, int backup_slot, int index, const PatternProjectSlot& state);
bool get_memory_card_backup_pattern_events(const Device* dev, int backup_slot, int index, PatternProjectEvent* out_events, int max_events, int* out_count);
bool set_memory_card_backup_pattern_events(Device* dev, int backup_slot, int index, const PatternProjectEvent* events, int count);

// Pad sample state (for UI display)
bool pad_has_sample(const Device* dev, int pad_index);  // 0-31
void pad_clear_sample(Device* dev, int pad_index);
bool get_pad_project_state(const Device* dev, int pad_index, PadProjectState* out);
void set_pad_project_state(Device* dev, int pad_index, const PadProjectState& state);
void reset_project_state(Device* dev);
void set_sample_level_threshold(Device* dev, int level);
void set_time_bpm_display_number(Device* dev, int value);
void set_time_bpm_display_off(Device* dev);
void set_time_bpm_display_pattern(Device* dev);
void set_pad_led_hold_frames(Device* dev, int pad_index, int frames);
void clear_input_gain_display(Device* dev);

// Effect routing query
int  get_active_effect_btn(const Device* dev);               // -1 or ButtonID of globally active effect
bool get_pad_has_effect(const Device* dev, int pad_index);   // true if pad carries the active effect
void set_active_effect_btn(Device* dev, int btn_id);         // -1 or ButtonID of globally active effect
int  get_mfx_type(const Device* dev);                        // 0-20 (MFX sub-type)

} // namespace sp303
