#include "pattern_sequencer.h"

#include <algorithm>
#include <cmath>

namespace sp303 {

static int pattern_length_ticks(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) {
        return PatternSequencer::TICKS_PER_MEASURE;
    }
    return std::max(1, seq->slots[slot].length_measures) * PatternSequencer::TICKS_PER_MEASURE;
}

static int quantize_step_ticks(PatternQuantize q) {
    switch (q) {
        case PATTERN_QUANTIZE_4:  return PatternSequencer::TICKS_PER_QUARTER;
        case PATTERN_QUANTIZE_8:  return PatternSequencer::TICKS_PER_QUARTER / 2;
        case PATTERN_QUANTIZE_8T: return PatternSequencer::TICKS_PER_QUARTER / 3;
        case PATTERN_QUANTIZE_16: return PatternSequencer::TICKS_PER_QUARTER / 4;
        case PATTERN_QUANTIZE_OFF:
        default:                  return 1;
    }
}

static void enqueue_events_between(PatternSequencer* seq, int slot, int start_tick, int end_tick) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    const auto& pattern = seq->slots[slot];
    if (!pattern.assigned) return;
    for (const auto& ev : pattern.events) {
        if (ev.tick >= start_tick && ev.tick < end_tick) {
            seq->pending_triggers.push_back(ev.sample_pad);
        }
    }
}

static void enqueue_tick_zero_events(PatternSequencer* seq, int slot) {
    enqueue_events_between(seq, slot, 0, 1);
}

static void sort_events(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    auto& events = seq->slots[slot].events;
    std::sort(events.begin(), events.end(), [](const PatternEvent& a, const PatternEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.sample_pad < b.sample_pad;
    });
}

static void refresh_assigned_flag(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot].assigned = !seq->slots[slot].events.empty();
}

static void erase_events_between(PatternSequencer* seq, int slot, int start_tick, int end_tick, uint32_t erase_mask) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    if (erase_mask == 0 || start_tick >= end_tick) return;
    auto& events = seq->slots[slot].events;
    events.erase(std::remove_if(events.begin(), events.end(),
        [start_tick, end_tick, erase_mask](const PatternEvent& ev) {
            if (ev.tick < start_tick || ev.tick >= end_tick) return false;
            if (ev.sample_pad < 0 || ev.sample_pad >= 32) return false;
            return (erase_mask & (1u << ev.sample_pad)) != 0;
        }),
        events.end());
    refresh_assigned_flag(seq, slot);
}

static void enqueue_metronome_between(PatternSequencer* seq, double prev_tick, double next_tick) {
    if (!seq || !seq->recording) return;
    int prev_q = (int)std::floor(prev_tick / PatternSequencer::TICKS_PER_QUARTER);
    int next_q = (int)std::floor(next_tick / PatternSequencer::TICKS_PER_QUARTER);
    for (int q = prev_q + 1; q <= next_q; ++q) {
        int beat_in_bar = ((q % 4) + 4) % 4;
        seq->pending_metronome.push_back(beat_in_bar == 0 ? PATTERN_METRO_STRONG : PATTERN_METRO_WEAK);
    }
}

void pattern_init(PatternSequencer* seq) {
    if (!seq) return;
    *seq = {};
    seq->bpm = 120;
    for (auto& slot : seq->slots) {
        slot.length_measures = 1;
        slot.quantize = PATTERN_QUANTIZE_16;
        slot.metronome_level = 100;
    }
}

void pattern_reset(PatternSequencer* seq) {
    pattern_init(seq);
}

bool pattern_has_data(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return false;
    return seq->slots[slot].assigned;
}

int pattern_get_bpm(const PatternSequencer* seq) {
    if (!seq) return 120;
    return std::clamp(seq->bpm, 40, 200);
}

void pattern_set_bpm(PatternSequencer* seq, int bpm) {
    if (!seq) return;
    seq->bpm = std::clamp(bpm, 40, 200);
}

bool pattern_is_playing(const PatternSequencer* seq) {
    return seq && seq->playing;
}

bool pattern_is_recording(const PatternSequencer* seq) {
    return seq && seq->recording;
}

bool pattern_is_count_in(const PatternSequencer* seq) {
    return seq && seq->count_in;
}

int pattern_get_length_measures(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return 1;
    return std::clamp(seq->slots[slot].length_measures, 1, 99);
}

PatternQuantize pattern_get_quantize(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return PATTERN_QUANTIZE_16;
    return seq->slots[slot].quantize;
}

int pattern_get_metronome_level(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return 100;
    return std::clamp(seq->slots[slot].metronome_level, 0, 127);
}

void pattern_set_length_measures(PatternSequencer* seq, int slot, int measures) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot].length_measures = std::clamp(measures, 1, 99);
}

void pattern_set_quantize(PatternSequencer* seq, int slot, PatternQuantize q) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot].quantize = q;
}

void pattern_set_metronome_level(PatternSequencer* seq, int slot, int level) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot].metronome_level = std::clamp(level, 0, 127);
}

int pattern_get_event_count(const PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return 0;
    return (int)seq->slots[slot].events.size();
}

bool pattern_get_event(const PatternSequencer* seq, int slot, int index, PatternEvent* out) {
    if (!seq || !out || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return false;
    const auto& events = seq->slots[slot].events;
    if (index < 0 || index >= (int)events.size()) return false;
    *out = events[(size_t)index];
    return true;
}

void pattern_clear_events(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot].events.clear();
    refresh_assigned_flag(seq, slot);
}

bool pattern_append_event(PatternSequencer* seq, int slot, const PatternEvent& event) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return false;
    if (event.sample_pad < 0 || event.sample_pad >= PatternSequencer::SLOT_COUNT) return false;
    int length_ticks = pattern_length_ticks(seq, slot);
    PatternEvent clamped = event;
    clamped.tick = std::clamp(clamped.tick, 0, std::max(0, length_ticks - 1));
    seq->slots[slot].events.push_back(clamped);
    seq->slots[slot].assigned = true;
    sort_events(seq, slot);
    return true;
}

void pattern_clear_slot(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->slots[slot] = {};
    seq->slots[slot].length_measures = 1;
    seq->slots[slot].quantize = PATTERN_QUANTIZE_16;
    seq->slots[slot].metronome_level = 100;
    if (seq->current_slot == slot && !seq->recording) {
        pattern_stop(seq);
    } else if (seq->record_slot == slot) {
        seq->pending_triggers.clear();
        refresh_assigned_flag(seq, slot);
    }
}

void pattern_clear_range(PatternSequencer* seq, int start_slot, int end_slot) {
    if (!seq) return;
    int start = std::clamp(start_slot, 0, PatternSequencer::SLOT_COUNT);
    int end = std::clamp(end_slot, 0, PatternSequencer::SLOT_COUNT);
    for (int slot = start; slot < end; ++slot) {
        pattern_clear_slot(seq, slot);
    }
}

void pattern_swap_slots(PatternSequencer* seq, int slot_a, int slot_b) {
    if (!seq || slot_a < 0 || slot_a >= PatternSequencer::SLOT_COUNT ||
        slot_b < 0 || slot_b >= PatternSequencer::SLOT_COUNT ||
        slot_a == slot_b) {
        return;
    }
    std::swap(seq->slots[slot_a], seq->slots[slot_b]);
    if (seq->current_slot == slot_a) seq->current_slot = slot_b;
    else if (seq->current_slot == slot_b) seq->current_slot = slot_a;
    if (seq->record_slot == slot_a) seq->record_slot = slot_b;
    else if (seq->record_slot == slot_b) seq->record_slot = slot_a;
}

void pattern_set_erase_pad(PatternSequencer* seq, int sample_pad, bool enabled) {
    if (!seq || sample_pad < 0 || sample_pad >= 32) return;
    uint32_t bit = (1u << sample_pad);
    if (enabled) seq->erase_pad_mask |= bit;
    else seq->erase_pad_mask &= ~bit;
}

int pattern_get_current_slot(const PatternSequencer* seq) {
    return seq ? seq->current_slot : -1;
}

int pattern_get_record_slot(const PatternSequencer* seq) {
    return seq ? seq->record_slot : -1;
}

int pattern_get_current_measure(const PatternSequencer* seq) {
    if (!seq || seq->current_slot < 0) return 1;
    int length_ticks = pattern_length_ticks(seq, seq->current_slot);
    if (length_ticks <= 0) return 1;
    int tick = std::clamp((int)std::floor(seq->tick_position), 0, std::max(length_ticks - 1, 0));
    return 1 + (tick / PatternSequencer::TICKS_PER_MEASURE);
}

int pattern_get_current_beat(const PatternSequencer* seq) {
    if (!seq || seq->current_slot < 0) return 1;
    int length_ticks = pattern_length_ticks(seq, seq->current_slot);
    if (length_ticks <= 0) return 1;
    int tick = std::clamp((int)std::floor(seq->tick_position), 0, std::max(length_ticks - 1, 0));
    int measure_tick = tick % PatternSequencer::TICKS_PER_MEASURE;
    return 1 + (measure_tick / PatternSequencer::TICKS_PER_QUARTER);
}

int pattern_get_count_in_beat(const PatternSequencer* seq) {
    if (!seq || !seq->count_in) return 0;
    int quarters = std::max(0, seq->count_in_ticks_remaining) / PatternSequencer::TICKS_PER_QUARTER;
    return std::clamp(quarters + 1, 1, 4);
}

void pattern_start_playback(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    if (!seq->slots[slot].assigned) return;
    seq->playing = true;
    seq->recording = false;
    seq->count_in = false;
    seq->current_slot = slot;
    seq->record_slot = -1;
    seq->tick_position = 0.0;
    seq->count_in_ticks_remaining = 0;
    seq->last_quarter_index = -1;
    seq->erase_pad_mask = 0;
    seq->pending_triggers.clear();
    seq->pending_metronome.clear();
    enqueue_tick_zero_events(seq, slot);
}

void pattern_stop(PatternSequencer* seq) {
    if (!seq) return;
    seq->playing = false;
    seq->recording = false;
    seq->count_in = false;
    seq->current_slot = -1;
    seq->record_slot = -1;
    seq->tick_position = 0.0;
    seq->count_in_ticks_remaining = 0;
    seq->last_quarter_index = -1;
    seq->erase_pad_mask = 0;
    seq->pending_triggers.clear();
    seq->pending_metronome.clear();
}

void pattern_start_record(PatternSequencer* seq, int slot) {
    if (!seq || slot < 0 || slot >= PatternSequencer::SLOT_COUNT) return;
    seq->current_slot = slot;
    seq->record_slot = slot;
    seq->playing = true;
    seq->recording = true;
    seq->count_in = true;
    seq->tick_position = 0.0;
    seq->count_in_ticks_remaining = PatternSequencer::TICKS_PER_MEASURE;
    seq->last_quarter_index = -1;
    seq->erase_pad_mask = 0;
    seq->pending_triggers.clear();
    seq->pending_metronome.clear();
    seq->pending_metronome.push_back(PATTERN_METRO_STRONG);
}

void pattern_stop_record(PatternSequencer* seq) {
    if (!seq) return;
    if (seq->record_slot >= 0 && seq->record_slot < PatternSequencer::SLOT_COUNT) {
        auto& slot = seq->slots[seq->record_slot];
        slot.assigned = !slot.events.empty() || slot.assigned;
        sort_events(seq, seq->record_slot);
    }
    seq->recording = false;
    seq->count_in = false;
    seq->record_slot = -1;
    seq->erase_pad_mask = 0;
    seq->pending_metronome.clear();
}

void pattern_record_pad_hit(PatternSequencer* seq, int sample_pad) {
    if (!seq || !seq->recording || seq->count_in) return;
    if (seq->record_slot < 0 || seq->record_slot >= PatternSequencer::SLOT_COUNT) return;
    if (sample_pad < 0 || sample_pad >= PatternSequencer::SLOT_COUNT) return;

    auto& slot = seq->slots[seq->record_slot];
    const int length_ticks = pattern_length_ticks(seq, seq->record_slot);
    const int step = quantize_step_ticks(slot.quantize);
    int tick = (int)std::lround(seq->tick_position);
    if (step > 1) {
        tick = (int)std::lround(tick / (double)step) * step;
    }
    tick %= length_ticks;
    if (tick < 0) tick += length_ticks;

    slot.events.push_back({tick, sample_pad});
    slot.assigned = true;
    sort_events(seq, seq->record_slot);
}

void pattern_advance(PatternSequencer* seq, uint32_t samples_elapsed, uint32_t sample_rate) {
    if (!seq || !seq->playing) return;
    const int bpm = pattern_get_bpm(seq);
    const double ticks_per_second = (bpm / 60.0) * PatternSequencer::TICKS_PER_QUARTER;
    const double ticks_delta = (samples_elapsed / (double)std::max(sample_rate, 1u)) * ticks_per_second;

    if (seq->count_in) {
        double prev_count = (double)PatternSequencer::TICKS_PER_MEASURE - seq->count_in_ticks_remaining;
        double next_count = prev_count + ticks_delta;
        enqueue_metronome_between(seq, prev_count, std::min(next_count, (double)PatternSequencer::TICKS_PER_MEASURE));
        seq->count_in_ticks_remaining -= (int)std::floor(ticks_delta);
        if (seq->count_in_ticks_remaining <= 0) {
            seq->count_in = false;
            seq->count_in_ticks_remaining = 0;
            seq->tick_position = 0.0;
            seq->last_quarter_index = -1;
            if (seq->current_slot >= 0) {
                enqueue_tick_zero_events(seq, seq->current_slot);
            }
        }
        return;
    }

    if (seq->current_slot < 0 || seq->current_slot >= PatternSequencer::SLOT_COUNT) {
        pattern_stop(seq);
        return;
    }

    const int length_ticks = pattern_length_ticks(seq, seq->current_slot);
    const double prev = seq->tick_position;
    double next = prev + ticks_delta;
    enqueue_metronome_between(seq, prev, next);

    while (next >= length_ticks) {
        if (seq->recording && seq->erase_pad_mask != 0) {
            erase_events_between(seq, seq->current_slot, (int)std::floor(prev), length_ticks, seq->erase_pad_mask);
            erase_events_between(seq, seq->current_slot, 0, (int)std::floor(next - length_ticks), seq->erase_pad_mask);
        }
        enqueue_events_between(seq, seq->current_slot, (int)std::floor(prev), length_ticks);
        next -= length_ticks;
        enqueue_events_between(seq, seq->current_slot, 0, (int)std::floor(next));
        seq->tick_position = next;
        if (!seq->recording && !seq->slots[seq->current_slot].assigned) {
            pattern_stop(seq);
            return;
        }
        return;
    }

    if (seq->recording && seq->erase_pad_mask != 0) {
        erase_events_between(seq, seq->current_slot, (int)std::floor(prev), (int)std::floor(next), seq->erase_pad_mask);
    }
    enqueue_events_between(seq, seq->current_slot, (int)std::floor(prev), (int)std::floor(next));
    seq->tick_position = next;
}

int pattern_consume_trigger(PatternSequencer* seq) {
    if (!seq || seq->pending_triggers.empty()) return -1;
    int out = seq->pending_triggers.front();
    seq->pending_triggers.erase(seq->pending_triggers.begin());
    return out;
}

int pattern_consume_metronome(PatternSequencer* seq) {
    if (!seq || seq->pending_metronome.empty()) return PATTERN_METRO_NONE;
    int out = seq->pending_metronome.front();
    seq->pending_metronome.erase(seq->pending_metronome.begin());
    return out;
}

} // namespace sp303
