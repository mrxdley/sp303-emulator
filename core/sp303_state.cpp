#include "sp303_internal.h"

#include <algorithm>
#include <cstring>

namespace sp303 {

void clear_pattern_edit_modes(Device* dev) {
    if (!dev) return;
    dev->pattern_bpm_edit_mode = false;
    dev->pattern_metronome_edit_mode = false;
    dev->pattern_length_edit_mode = false;
    dev->pattern_quantize_edit_mode = false;
}

void clear_pattern_management_modes(Device* dev) {
    if (!dev) return;
    dev->pattern_erase_mode = false;
    dev->pattern_delete_select = false;
    dev->pattern_delete_all_select = false;
    dev->pattern_delete_all_armed = false;
    dev->pattern_swap_select_a = false;
    dev->pattern_swap_select_b = false;
    dev->pattern_delete_slot = -1;
    dev->pattern_delete_all_group = -1;
    dev->pattern_swap_a = -1;
    dev->pattern_swap_b = -1;
    for (int i = 0; i < 32; ++i) {
        pattern_set_erase_pad(&dev->pattern_seq, i, false);
    }
}

void sync_memory_card_live_sample(Device* dev, int pad_index) {
    if (!dev || pad_index < 16 || pad_index >= 32) return;
    int idx = pad_index - 16;
    auto& dst = dev->memory_card.live_sample_slots_cd[(size_t)idx];
    dst.has_sample = dev->pad_has_sample[pad_index];
    dst.state.has_sample = dev->pad_has_sample[pad_index];
    dst.state.loop_mode = dev->pad_loop_mode[pad_index];
    dst.state.gate_mode = dev->pad_gate_mode[pad_index];
    dst.state.reverse_mode = dev->pad_reverse_mode[pad_index];
    dst.state.has_effect = dev->pad_has_effect[pad_index];
    dst.state.recorded_stereo = dev->pad_recorded_stereo[pad_index];
    dst.state.recorded_quality = dev->pad_recorded_quality[pad_index];
    dev->memory_card_dirty = true;
}

void sync_memory_card_live_pattern(Device* dev, int slot) {
    if (!dev || slot < 16 || slot >= 32) return;
    int idx = slot - 16;
    auto& dst = dev->memory_card.live_pattern_slots_cd[(size_t)idx];
    dst.slot.assigned = pattern_has_data(&dev->pattern_seq, slot);
    dst.slot.length_measures = pattern_get_length_measures(&dev->pattern_seq, slot);
    dst.slot.quantize = (int)pattern_get_quantize(&dev->pattern_seq, slot);
    dst.slot.metronome_level = pattern_get_metronome_level(&dev->pattern_seq, slot);
    int count = pattern_get_event_count(&dev->pattern_seq, slot);
    dst.events.clear();
    if (count > 0) {
        dst.events.reserve((size_t)count);
        for (int i = 0; i < count; ++i) {
            PatternEvent ev{};
            if (pattern_get_event(&dev->pattern_seq, slot, i, &ev)) {
                dst.events.push_back({ev.type, ev.tick, ev.sample_pad, ev.velocity});
            }
        }
    }
    dev->memory_card_dirty = true;
}

bool get_pattern_project_slot(const Device* dev, int slot, PatternProjectSlot* out) {
    if (!dev || !out || slot < 0 || slot >= 32) return false;
    out->assigned = pattern_has_data(&dev->pattern_seq, slot);
    out->length_measures = pattern_get_length_measures(&dev->pattern_seq, slot);
    out->quantize = (int)pattern_get_quantize(&dev->pattern_seq, slot);
    out->metronome_level = pattern_get_metronome_level(&dev->pattern_seq, slot);
    return true;
}

bool set_pattern_project_slot(Device* dev, int slot, const PatternProjectSlot& state) {
    if (!dev || slot < 0 || slot >= 32) return false;
    pattern_set_length_measures(&dev->pattern_seq, slot, state.length_measures);
    pattern_set_quantize(&dev->pattern_seq, slot, (PatternQuantize)std::clamp(state.quantize, 0, 4));
    pattern_set_metronome_level(&dev->pattern_seq, slot, state.metronome_level);
    if (!state.assigned) {
        pattern_clear_events(&dev->pattern_seq, slot);
    }
    sync_memory_card_live_pattern(dev, slot);
    return true;
}

bool get_pattern_project_events(const Device* dev, int slot, PatternProjectEvent* out_events, int max_events, int* out_count) {
    if (!dev || slot < 0 || slot >= 32 || !out_count) return false;
    int count = pattern_get_event_count(&dev->pattern_seq, slot);
    *out_count = count;
    if (!out_events || max_events <= 0) return true;
    int n = std::min(count, max_events);
    for (int i = 0; i < n; ++i) {
        PatternEvent ev{};
        if (!pattern_get_event(&dev->pattern_seq, slot, i, &ev)) return false;
        out_events[i].type = ev.type;
        out_events[i].tick = ev.tick;
        out_events[i].sample_pad = ev.sample_pad;
        out_events[i].velocity = ev.velocity;
    }
    return true;
}

bool set_pattern_project_events(Device* dev, int slot, const PatternProjectEvent* events, int count) {
    if (!dev || slot < 0 || slot >= 32 || count < 0) return false;
    pattern_clear_events(&dev->pattern_seq, slot);
    for (int i = 0; i < count; ++i) {
        PatternEvent ev{};
        ev.type = events[i].type;
        ev.tick = events[i].tick;
        ev.sample_pad = events[i].sample_pad;
        ev.velocity = events[i].velocity;
        if (!pattern_append_event(&dev->pattern_seq, slot, ev)) return false;
    }
    sync_memory_card_live_pattern(dev, slot);
    return true;
}

bool get_memory_card_formatted(const Device* dev) {
    return dev ? dev->memory_card.formatted : false;
}

void set_memory_card_formatted(Device* dev, bool formatted) {
    if (!dev) return;
    dev->memory_card.formatted = formatted;
    dev->memory_card_dirty = true;
}

bool get_memory_card_write_protected(const Device* dev) {
    return dev ? dev->memory_card.write_protected : false;
}

void set_memory_card_write_protected(Device* dev, bool write_protected) {
    if (!dev) return;
    dev->memory_card.write_protected = write_protected;
    dev->memory_card_dirty = true;
}

int get_memory_card_backup_kind(const Device* dev, int backup_slot) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return MEMORY_CARD_BACKUP_EMPTY;
    return (int)dev->memory_card.backup_slots[(size_t)backup_slot].kind;
}

void set_memory_card_backup_kind(Device* dev, int backup_slot, int kind) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return;
    dev->memory_card.backup_slots[(size_t)backup_slot].kind =
        (MemoryCardBackupKind)std::clamp(kind, (int)MEMORY_CARD_BACKUP_EMPTY, (int)MEMORY_CARD_BACKUP_PATTERNS);
    dev->memory_card_dirty = true;
}

int get_memory_card_backup_pattern_bpm(const Device* dev, int backup_slot) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return 120;
    return std::clamp(dev->memory_card.backup_slots[(size_t)backup_slot].pattern_bpm, 40, 200);
}

void set_memory_card_backup_pattern_bpm(Device* dev, int backup_slot, int bpm) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return;
    dev->memory_card.backup_slots[(size_t)backup_slot].pattern_bpm = std::clamp(bpm, 40, 200);
    dev->memory_card_dirty = true;
}

bool get_memory_card_backup_sample_state(const Device* dev, int backup_slot, int index, PadProjectState* out) {
    if (!dev || !out || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    *out = dev->memory_card.backup_slots[(size_t)backup_slot].sample_states[(size_t)index];
    return true;
}

bool set_memory_card_backup_sample_state(Device* dev, int backup_slot, int index, const PadProjectState& state) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    dev->memory_card.backup_slots[(size_t)backup_slot].sample_states[(size_t)index] = state;
    dev->memory_card_dirty = true;
    return true;
}

bool get_memory_card_backup_sample_audio(const Device* dev, int backup_slot, int index, std::vector<float>* pcm, uint32_t* channels, uint32_t* frames) {
    if (!dev || !pcm || !channels || !frames || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    const auto& src = dev->memory_card.backup_slots[(size_t)backup_slot].sample_data[(size_t)index];
    *pcm = src.pcm;
    *channels = src.channels;
    *frames = src.frames;
    return true;
}

bool set_memory_card_backup_sample_audio(Device* dev, int backup_slot, int index, const float* pcm, uint32_t frames, uint32_t channels) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    auto& dst = dev->memory_card.backup_slots[(size_t)backup_slot].sample_data[(size_t)index];
    dst.channels = std::max(1u, channels);
    dst.frames = frames;
    if (!pcm || frames == 0) {
        dst.pcm.clear();
        dst.frames = 0;
        dev->memory_card_dirty = true;
        return true;
    }
    dst.pcm.assign(pcm, pcm + ((size_t)frames * (size_t)dst.channels));
    dev->memory_card_dirty = true;
    return true;
}

bool get_memory_card_backup_sample_edit(const Device* dev, int backup_slot, int index,
                                        int* start_127, int* end_127, int* level_127,
                                        int* bpm_adjust, int* time_mode, int* time_target_bpm) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    const auto& src = dev->memory_card.backup_slots[(size_t)backup_slot].sample_data[(size_t)index];
    if (start_127) *start_127 = src.start_127;
    if (end_127) *end_127 = src.end_127;
    if (level_127) *level_127 = src.level_127;
    if (bpm_adjust) *bpm_adjust = src.bpm_adjust;
    if (time_mode) *time_mode = src.time_mode;
    if (time_target_bpm) *time_target_bpm = src.time_target_bpm;
    return true;
}

bool set_memory_card_backup_sample_edit(Device* dev, int backup_slot, int index,
                                        int start_127, int end_127, int level_127,
                                        int bpm_adjust, int time_mode, int time_target_bpm) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    auto& dst = dev->memory_card.backup_slots[(size_t)backup_slot].sample_data[(size_t)index];
    dst.start_127 = std::clamp(start_127, 0, 127);
    dst.end_127 = std::clamp(end_127, 0, 127);
    dst.level_127 = std::clamp(level_127, 0, 127);
    dst.bpm_adjust = std::clamp(bpm_adjust, -1, 1);
    dst.time_mode = std::clamp(time_mode, 0, 2);
    dst.time_target_bpm = time_target_bpm;
    dev->memory_card_dirty = true;
    return true;
}

bool get_memory_card_backup_pattern_slot(const Device* dev, int backup_slot, int index, PatternProjectSlot* out) {
    if (!dev || !out || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    *out = dev->memory_card.backup_slots[(size_t)backup_slot].pattern_slots[(size_t)index];
    return true;
}

bool set_memory_card_backup_pattern_slot(Device* dev, int backup_slot, int index, const PatternProjectSlot& state) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16) return false;
    dev->memory_card.backup_slots[(size_t)backup_slot].pattern_slots[(size_t)index] = state;
    dev->memory_card_dirty = true;
    return true;
}

bool get_memory_card_backup_pattern_events(const Device* dev, int backup_slot, int index, PatternProjectEvent* out_events, int max_events, int* out_count) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16 || !out_count) return false;
    const auto& events = dev->memory_card.backup_slots[(size_t)backup_slot].pattern_events[(size_t)index];
    *out_count = (int)events.size();
    if (!out_events || max_events <= 0) return true;
    int n = std::min((int)events.size(), max_events);
    for (int i = 0; i < n; ++i) {
        out_events[i].tick = events[(size_t)i].tick;
        out_events[i].sample_pad = events[(size_t)i].sample_pad;
        out_events[i].velocity = events[(size_t)i].velocity;
    }
    return true;
}

bool set_memory_card_backup_pattern_events(Device* dev, int backup_slot, int index, const PatternProjectEvent* events, int count) {
    if (!dev || backup_slot < 0 || backup_slot >= 8 || index < 0 || index >= 16 || count < 0) return false;
    auto& dst = dev->memory_card.backup_slots[(size_t)backup_slot].pattern_events[(size_t)index];
    dst.clear();
    dst.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
        dst.push_back({events[i].type, events[i].tick, events[i].sample_pad, events[i].velocity});
    }
    dev->memory_card_dirty = true;
    return true;
}

void pad_clear_sample(Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_has_sample[pad_index] = false;
    dev->pad_loop_mode[pad_index] = false;
    dev->pad_gate_mode[pad_index] = false;
    dev->pad_reverse_mode[pad_index] = false;
    dev->pad_has_effect[pad_index] = false;
    dev->pad_recorded_stereo[pad_index] = false;
    dev->pad_recorded_quality[pad_index] = SAMPLE_QUALITY_STANDARD;
    dev->pad_led_hold_frames[pad_index] = 0;
    dev->pad_is_playing[pad_index] = false;
    dev->pad_is_marked[pad_index] = false;
    sync_memory_card_live_sample(dev, pad_index);
}

bool pad_has_sample(const Device* dev, int pad_index) {
    if (!dev || pad_index < 0 || pad_index >= 32) return false;
    return dev->pad_has_sample[pad_index];
}

bool get_pad_project_state(const Device* dev, int pad_index, PadProjectState* out) {
    if (!dev || !out || pad_index < 0 || pad_index >= 32) return false;
    out->has_sample = dev->pad_has_sample[pad_index];
    out->loop_mode = dev->pad_loop_mode[pad_index];
    out->gate_mode = dev->pad_gate_mode[pad_index];
    out->reverse_mode = dev->pad_reverse_mode[pad_index];
    out->has_effect = dev->pad_has_effect[pad_index];
    out->recorded_stereo = dev->pad_recorded_stereo[pad_index];
    out->recorded_quality = dev->pad_recorded_quality[pad_index];
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
    dev->pad_recorded_stereo[pad_index] = state.recorded_stereo;
    dev->pad_recorded_quality[pad_index] = std::clamp(state.recorded_quality, (int)SAMPLE_QUALITY_STANDARD, (int)SAMPLE_QUALITY_LOFI);
    sync_memory_card_live_sample(dev, pad_index);
}

void reset_project_state(Device* dev) {
    if (!dev) return;
    std::memset(dev->state.buttons, 0, sizeof(dev->state.buttons));
    std::memset(dev->state.indicators, 0, sizeof(dev->state.indicators));
    for (int i = 0; i < 32; ++i) {
        pad_clear_sample(dev, i);
        dev->pad_led_hold_frames[i] = 0;
        dev->pad_is_marked[i] = false;
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
    dev->tap_count = 0;
    std::memset(dev->tap_times, 0, sizeof(dev->tap_times));
    dev->pending_record_bpm_quantize = -1;
    dev->rec_gain_display_frames = 0;
    dev->rec_gain_value = 0;
    dev->active_effect_btn = -1;
    pattern_reset(&dev->pattern_seq);
    memory_card_reset(&dev->memory_card);
    dev->state.active_bank = 0;
    dev->state.buttons[BTN_BANK_A].lit = true;
    display_raw(dev, SEG_DASH_I, SEG_DASH_I, SEG_DASH_I);
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
    display_raw(dev, SEG_PTN_I[0], SEG_PTN_I[1], SEG_PTN_I[2]);
}

void set_pad_led_hold_frames(Device* dev, int pad_index, int frames) {
    if (!dev || pad_index < 0 || pad_index >= 32) return;
    dev->pad_led_hold_frames[pad_index] = std::max(frames, 0);
}

void clear_input_gain_display(Device* dev) {
    if (!dev) return;
    dev->rec_gain_display_frames = 0;
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
