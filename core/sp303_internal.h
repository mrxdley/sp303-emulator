#pragma once

#include "sp303.h"
#include "pattern_sequencer.h"
#include "memory_card.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sp303 {

inline constexpr uint32_t CORE_TAP_SAMPLE_RATE = 44100;
inline constexpr uint32_t TAP_TIMEOUT_SAMPLES = CORE_TAP_SAMPLE_RATE * 2;

inline constexpr uint8_t SEG_FMT_I[3] = {
    SEG_A | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,
    SEG_D | SEG_E | SEG_F | SEG_G,
};

inline constexpr uint8_t SEG_EMP_I[3] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F,
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
};

inline constexpr uint8_t SEG_PRT_I[3] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
    SEG_E | SEG_G,
    SEG_D | SEG_E | SEG_F | SEG_G,
};

inline constexpr uint8_t SEG_PTN_I[3] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
    SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_C | SEG_E | SEG_G
};

inline constexpr uint8_t SEG_DASH_I = SEG_G;

enum SamplingState {
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
    SAMPLING_MARK_EDIT,
    SAMPLING_MARK_END_ONLY,
    SAMPLING_RESAMPLE_SOURCE,
    SAMPLING_RESAMPLE_DEST,
    SAMPLING_RESAMPLE_ARMED,
    SAMPLING_RESAMPLE_RECORDING,
    SAMPLING_CARD_FORMAT_SELECT,
    SAMPLING_CARD_FORMAT_ARMED,
    SAMPLING_CARD_SAMPLE_SAVE_SELECT,
    SAMPLING_CARD_SAMPLE_SAVE_ARMED,
    SAMPLING_CARD_SAMPLE_LOAD_SELECT,
    SAMPLING_CARD_SAMPLE_LOAD_ARMED,
    SAMPLING_CARD_PATTERN_SAVE_SELECT,
    SAMPLING_CARD_PATTERN_SAVE_ARMED,
    SAMPLING_CARD_PATTERN_LOAD_SELECT,
    SAMPLING_CARD_PATTERN_LOAD_ARMED,
};

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
    int memory_card_selected_bank;
    int memory_card_backup_slot;
    bool memory_card_format_pending;
    int memory_card_sample_save_pending_slot;
    int memory_card_sample_load_pending_slot;
    bool memory_card_dirty;
    int transient_display_frames;
    uint8_t transient_display[3];
    bool card_emp_lock;
    bool card_emp_return_pattern;
    uint8_t card_emp_return_bank;
    uint8_t card_emp_group;
    int mark_edit_pad;
    int mark_pending_action;
    int mark_action_pad;
    int mark_end_only_lock_pad;
    bool sampling_stereo;
    SampleQuality sampling_quality;
    bool bpm_edit_mode;
    int bpm_value;
    bool bpm_armed_for_next_sample;
    bool time_bpm_mode;
    int time_bpm_pad;
    bool pattern_mode;
    bool pattern_bpm_edit_mode;
    bool pattern_record_select;
    bool pattern_recording;
    bool pattern_erase_mode;
    bool pattern_metronome_edit_mode;
    bool pattern_length_edit_mode;
    bool pattern_quantize_edit_mode;
    bool pattern_delete_select;
    bool pattern_delete_all_select;
    bool pattern_delete_all_armed;
    bool pattern_swap_select_a;
    bool pattern_swap_select_b;
    int pattern_record_slot;
    int pattern_delete_slot;
    int pattern_delete_all_group;
    int pattern_swap_a;
    int pattern_swap_b;
    int pattern_tap_display_value;
    int pattern_tap_display_frames;
    uint32_t pattern_tap_times[4];
    int pattern_tap_count;
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
    bool pad_recorded_stereo[32];
    int  pad_recorded_quality[32];
    bool pad_is_playing[32];
    bool pad_is_marked[32];

    bool pad_has_sample[32];
    PatternSequencer pattern_seq;
    MemoryCardState memory_card;

    int  active_effect_btn;
    bool pad_has_effect[32];
    int  mfx_type;
    bool mfx_knob_touched_while_pressed;

    uint32_t blink_phase;
    bool blink_on;
};

bool pattern_ui_active(const Device* dev);
bool pattern_mode_active(const Device* dev);
void clear_pattern_edit_modes(Device* dev);
void clear_pattern_management_modes(Device* dev);

void clear_card_management_state(Device* dev);
void clear_bank_lights(Device* dev);
void show_transient_display(Device* dev, uint8_t d0, uint8_t d1, uint8_t d2, int frames);
void restore_active_bank_lights(Device* dev);
void finish_card_emp_lock(Device* dev);
void start_card_emp_lock(Device* dev, bool return_to_pattern_mode, uint8_t group);
bool try_enter_sample_card_workflow(Device* dev, ButtonID btn);
bool try_enter_pattern_card_workflow(Device* dev, ButtonID btn);

int button_to_actual_pad(const Device* dev, ButtonID btn);
int visible_pad_button_id(const Device* dev, int local_index);
void render_visible_sample_trigger_leds(Device* dev, int bank_start);
char pad_bank_name(int pad_index);
int pad_bank_number(int pad_index);
bool backup_slot_has_kind(const Device* dev, int kind);
void exit_card_workflow(Device* dev, bool return_to_pattern_mode);
void save_patterns_to_backup(Device* dev, int backup_slot);
void load_patterns_from_backup(Device* dev, int backup_slot);

int find_empty_pad(Device* dev);
int get_pad_button_id(int pad_index);
bool is_effect_btn(ButtonID btn);
void set_display_number_plain(Device* dev, int value);
void set_display_measure_beat(Device* dev, int measure, int beat);
void sync_memory_card_live_sample(Device* dev, int pad_index);
void sync_memory_card_live_pattern(Device* dev, int slot);
void tap_record(uint32_t* times, int* count, uint32_t now);

void finish_threshold_recording(Device* dev);

} // namespace sp303
