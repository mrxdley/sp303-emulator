#pragma once

#include <stdint.h>
#include <vector>

namespace sp303 {

struct PatternEvent {
    int tick = 0;
    int sample_pad = 0;
    int velocity = 127;
};

enum PatternQuantize {
    PATTERN_QUANTIZE_OFF = 0,
    PATTERN_QUANTIZE_4,
    PATTERN_QUANTIZE_8,
    PATTERN_QUANTIZE_8T,
    PATTERN_QUANTIZE_16,
};

enum PatternMetronomeAccent {
    PATTERN_METRO_NONE = 0,
    PATTERN_METRO_WEAK,
    PATTERN_METRO_STRONG,
};

struct PatternSlot {
    bool assigned = false;
    int length_measures = 1;
    PatternQuantize quantize = PATTERN_QUANTIZE_16;
    int metronome_level = 100;
    std::vector<PatternEvent> events;
};

struct PatternSequencer {
    static constexpr int SLOT_COUNT = 32;
    static constexpr int TICKS_PER_QUARTER = 96;
    static constexpr int TICKS_PER_MEASURE = TICKS_PER_QUARTER * 4;

    PatternSlot slots[SLOT_COUNT];
    int bpm = 120;

    bool playing = false;
    bool recording = false;
    bool count_in = false;
    int current_slot = -1;
    int record_slot = -1;
    double tick_position = 0.0;
    int count_in_ticks_remaining = 0;
    int last_quarter_index = -1;
    uint32_t erase_pad_mask = 0;

    std::vector<PatternEvent> pending_triggers;
    std::vector<int> pending_metronome;
};

void pattern_init(PatternSequencer* seq);
void pattern_reset(PatternSequencer* seq);

bool pattern_has_data(const PatternSequencer* seq, int slot);
int  pattern_get_bpm(const PatternSequencer* seq);
void pattern_set_bpm(PatternSequencer* seq, int bpm);

bool pattern_is_playing(const PatternSequencer* seq);
bool pattern_is_recording(const PatternSequencer* seq);
bool pattern_is_count_in(const PatternSequencer* seq);
int  pattern_get_current_slot(const PatternSequencer* seq);
int  pattern_get_record_slot(const PatternSequencer* seq);
int  pattern_get_current_measure(const PatternSequencer* seq);
int  pattern_get_current_beat(const PatternSequencer* seq);
int  pattern_get_count_in_beat(const PatternSequencer* seq);
int  pattern_get_length_measures(const PatternSequencer* seq, int slot);
PatternQuantize pattern_get_quantize(const PatternSequencer* seq, int slot);
int  pattern_get_metronome_level(const PatternSequencer* seq, int slot);
void pattern_set_length_measures(PatternSequencer* seq, int slot, int measures);
void pattern_set_quantize(PatternSequencer* seq, int slot, PatternQuantize q);
void pattern_set_metronome_level(PatternSequencer* seq, int slot, int level);
int  pattern_get_event_count(const PatternSequencer* seq, int slot);
bool pattern_get_event(const PatternSequencer* seq, int slot, int index, PatternEvent* out);
void pattern_clear_events(PatternSequencer* seq, int slot);
bool pattern_append_event(PatternSequencer* seq, int slot, const PatternEvent& event);
void pattern_clear_slot(PatternSequencer* seq, int slot);
void pattern_clear_range(PatternSequencer* seq, int start_slot, int end_slot);
void pattern_swap_slots(PatternSequencer* seq, int slot_a, int slot_b);
void pattern_set_erase_pad(PatternSequencer* seq, int sample_pad, bool enabled);

void pattern_start_playback(PatternSequencer* seq, int slot);
void pattern_stop(PatternSequencer* seq);
void pattern_start_record(PatternSequencer* seq, int slot);
void pattern_stop_record(PatternSequencer* seq);
void pattern_record_pad_hit(PatternSequencer* seq, int sample_pad, int velocity);
void pattern_advance(PatternSequencer* seq, uint32_t samples_elapsed, uint32_t sample_rate);
bool pattern_consume_trigger(PatternSequencer* seq, PatternEvent* out);
int  pattern_consume_metronome(PatternSequencer* seq);

} // namespace sp303
