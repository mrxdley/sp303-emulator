#include "sp303_internal.h"

namespace sp303 {

bool pattern_ui_active(const Device* dev) {
    return dev && (dev->pattern_mode ||
                   dev->pattern_record_select ||
                   dev->pattern_recording ||
                   dev->pattern_delete_select ||
                   dev->pattern_delete_all_select ||
                   dev->pattern_delete_all_armed ||
                   dev->pattern_swap_select_a ||
                   dev->pattern_swap_select_b);
}

bool pattern_mode_active(const Device* dev) {
    return dev && (pattern_ui_active(dev) || pattern_is_playing(&dev->pattern_seq));
}

void clear_card_management_state(Device* dev) {
    if (!dev) return;
    dev->memory_card_selected_bank = -1;
    dev->memory_card_backup_slot = -1;
}

void clear_bank_lights(Device* dev) {
    if (!dev) return;
    dev->state.buttons[BTN_BANK_A].lit = false;
    dev->state.buttons[BTN_BANK_B].lit = false;
    dev->state.buttons[BTN_BANK_C].lit = false;
    dev->state.buttons[BTN_BANK_D].lit = false;
}

void show_transient_display(Device* dev, uint8_t d0, uint8_t d1, uint8_t d2, int frames) {
    if (!dev) return;
    dev->transient_display[0] = d0;
    dev->transient_display[1] = d1;
    dev->transient_display[2] = d2;
    dev->transient_display_frames = std::max(frames, 0);
}

int button_to_actual_pad(const Device* dev, ButtonID btn) {
    if (!dev) return -1;
    if (btn >= BTN_PAD_1 && btn <= BTN_PAD_32) {
        return btn - BTN_PAD_1;
    }
    if (btn >= BTN_PAD_1 && btn <= BTN_PAD_8) {
        return dev->state.active_bank * 8 + (btn - BTN_PAD_1);
    }
    return -1;
}

int visible_pad_button_id(const Device* dev, int local_index) {
    if (!dev || local_index < 0 || local_index >= 8) return BTN_PAD_1;
    return BTN_PAD_1 + dev->state.active_bank * 8 + local_index;
}

void render_visible_sample_trigger_leds(Device* dev, int bank_start) {
    if (!dev) return;
    for (int i = 0; i < 8; ++i) {
        const int actual_pad = bank_start + i;
        const int visible_btn = visible_pad_button_id(dev, i);
        if (dev->pad_has_sample[actual_pad] && dev->pad_is_playing[actual_pad]) {
            dev->state.buttons[visible_btn].lit = true;
        }
    }
}

char pad_bank_name(int pad_index) {
    if (pad_index < 0 || pad_index >= 32) return '?';
    return "ABCD"[pad_index / 8];
}

int pad_bank_number(int pad_index) {
    if (pad_index < 0 || pad_index >= 32) return -1;
    return (pad_index % 8) + 1;
}

void restore_active_bank_lights(Device* dev) {
    if (!dev) return;
    for (int b = 0; b < 4; ++b) {
        dev->state.buttons[BTN_BANK_A + b].lit = (b == dev->state.active_bank);
    }
}

bool backup_slot_has_kind(const Device* dev, int kind) {
    if (!dev) return false;
    for (int i = 0; i < 8; ++i) {
        if (get_memory_card_backup_kind(dev, i) == kind) return true;
    }
    return false;
}

void exit_card_workflow(Device* dev, bool return_to_pattern_mode) {
    if (!dev) return;
    dev->sampling_state = SAMPLING_IDLE;
    clear_card_management_state(dev);
    restore_active_bank_lights(dev);
    if (return_to_pattern_mode) {
        dev->pattern_mode = true;
        display_raw(dev, SEG_PTN_I[0], SEG_PTN_I[1], SEG_PTN_I[2]);
    } else {
        display_raw(dev, SEG_DASH_I, SEG_DASH_I, SEG_DASH_I);
    }
}

void finish_card_emp_lock(Device* dev) {
    if (!dev) return;
    std::printf("[CARD] finish EMP lock -> return_pattern=%d return_bank=%c\n",
                dev->card_emp_return_pattern ? 1 : 0,
                "ABCD"[dev->card_emp_return_bank % 4]);
    dev->card_emp_lock = false;
    dev->sampling_state = SAMPLING_IDLE;
    dev->transient_display_frames = 0;
    for (int i = 0; i < 32; ++i) {
        dev->pad_led_hold_frames[i] = 0;
    }
    dev->state.active_bank = dev->card_emp_return_bank;
    restore_active_bank_lights(dev);
    if (dev->card_emp_return_pattern) {
        dev->pattern_mode = true;
        display_raw(dev, SEG_PTN_I[0], SEG_PTN_I[1], SEG_PTN_I[2]);
    } else {
        display_raw(dev, SEG_DASH_I, SEG_DASH_I, SEG_DASH_I);
    }
}

void start_card_emp_lock(Device* dev, bool return_to_pattern_mode, uint8_t group) {
    if (!dev) return;
    std::printf("[CARD] start EMP lock -> return_pattern=%d bank=%c frames=180\n",
                return_to_pattern_mode ? 1 : 0,
                "ABCD"[dev->state.active_bank % 4]);
    dev->card_emp_lock = true;
    dev->card_emp_return_pattern = return_to_pattern_mode;
    dev->card_emp_return_bank = dev->state.active_bank;
    dev->card_emp_group = group;
    dev->sampling_state = SAMPLING_IDLE;
    clear_card_management_state(dev);
    clear_bank_lights(dev);
    show_transient_display(dev, SEG_EMP_I[0], SEG_EMP_I[1], SEG_EMP_I[2], 180);
}

void save_patterns_to_backup(Device* dev, int backup_slot) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return;
    set_memory_card_backup_kind(dev, backup_slot, MEMORY_CARD_BACKUP_PATTERNS);
    set_memory_card_backup_pattern_bpm(dev, backup_slot, pattern_get_bpm(&dev->pattern_seq));
    for (int i = 0; i < 16; ++i) {
        PatternProjectSlot slot{};
        get_pattern_project_slot(dev, i, &slot);
        set_memory_card_backup_pattern_slot(dev, backup_slot, i, slot);
        int count = 0;
        get_pattern_project_events(dev, i, nullptr, 0, &count);
        std::vector<PatternProjectEvent> events((size_t)std::max(count, 0));
        if (count > 0) {
            get_pattern_project_events(dev, i, events.data(), count, &count);
        }
        set_memory_card_backup_pattern_events(dev, backup_slot, i, events.data(), count);
    }
}

void load_patterns_from_backup(Device* dev, int backup_slot) {
    if (!dev || backup_slot < 0 || backup_slot >= 8) return;
    pattern_stop_record(&dev->pattern_seq);
    pattern_stop(&dev->pattern_seq);
    dev->pattern_recording = false;
    dev->pattern_record_select = false;
    dev->pattern_record_slot = -1;
    clear_pattern_edit_modes(dev);
    clear_pattern_management_modes(dev);
    pattern_clear_range(&dev->pattern_seq, 0, 16);
    set_pattern_bpm(dev, get_memory_card_backup_pattern_bpm(dev, backup_slot));
    for (int i = 0; i < 16; ++i) {
        PatternProjectSlot slot{};
        get_memory_card_backup_pattern_slot(dev, backup_slot, i, &slot);
        set_pattern_project_slot(dev, i, slot);
        int count = 0;
        get_memory_card_backup_pattern_events(dev, backup_slot, i, nullptr, 0, &count);
        std::vector<PatternProjectEvent> events((size_t)std::max(count, 0));
        if (count > 0) {
            get_memory_card_backup_pattern_events(dev, backup_slot, i, events.data(), count, &count);
        }
        set_pattern_project_events(dev, i, events.data(), count);
    }
}

bool try_enter_sample_card_workflow(Device* dev, ButtonID btn) {
    if (!dev || dev->sampling_state != SAMPLING_IDLE || pattern_mode_active(dev)) return false;
    if (btn == BTN_CANCEL || btn == BTN_REMAIN || (btn >= BTN_BANK_A && btn <= BTN_BANK_D)) {
        std::printf("[CARD] sample workflow check btn=%d cancel_pressed=%d active_bank=%c\n",
                    (int)btn,
                    dev->state.buttons[BTN_CANCEL].pressed ? 1 : 0,
                    "ABCD"[dev->state.active_bank % 4]);
    }
    if (btn == BTN_REMAIN && dev->state.buttons[BTN_CANCEL].pressed) {
        std::printf("[CARD] sample chord CANCEL+REMAIN detected\n");
        if (get_memory_card_write_protected(dev)) {
            std::printf("[CARD] sample format blocked: write protected\n");
            display_raw(dev, SEG_PRT_I[0], SEG_PRT_I[1], SEG_PRT_I[2]);
            return true;
        }
        clear_card_management_state(dev);
        clear_bank_lights(dev);
        dev->sampling_state = SAMPLING_CARD_FORMAT_SELECT;
        display_raw(dev, SEG_FMT_I[0], SEG_FMT_I[1], SEG_FMT_I[2]);
        return true;
    }
    if (btn >= BTN_BANK_A && btn <= BTN_BANK_D && dev->state.buttons[BTN_CANCEL].pressed) {
        std::printf("[CARD] sample chord CANCEL+BANK detected bank=%c\n", "ABCD"[btn - BTN_BANK_A]);
        clear_card_management_state(dev);
        clear_bank_lights(dev);
        if (btn <= BTN_BANK_B) {
            bool has_samples = backup_slot_has_kind(dev, MEMORY_CARD_BACKUP_SAMPLES);
            std::printf("[CARD] sample load requested has_samples=%d\n", has_samples ? 1 : 0);
            if (!backup_slot_has_kind(dev, MEMORY_CARD_BACKUP_SAMPLES)) {
                start_card_emp_lock(dev, false, 0);
                return true;
            }
            std::printf("[CARD] entering sample load select\n");
            dev->sampling_state = SAMPLING_CARD_SAMPLE_LOAD_SELECT;
        } else {
            if (get_memory_card_write_protected(dev)) {
                std::printf("[CARD] sample save blocked: write protected\n");
                display_raw(dev, SEG_PRT_I[0], SEG_PRT_I[1], SEG_PRT_I[2]);
                return true;
            }
            std::printf("[CARD] entering sample save select\n");
            dev->sampling_state = SAMPLING_CARD_SAMPLE_SAVE_SELECT;
        }
        return true;
    }
    if (btn == BTN_CANCEL) {
        if (dev->state.buttons[BTN_REMAIN].pressed) {
            std::printf("[CARD] sample reverse-order chord via CANCEL with REMAIN held\n");
            return try_enter_sample_card_workflow(dev, BTN_REMAIN);
        }
        for (int bank = BTN_BANK_A; bank <= BTN_BANK_D; ++bank) {
            if (dev->state.buttons[bank].pressed) {
                std::printf("[CARD] sample reverse-order chord via CANCEL with bank %c held\n", "ABCD"[bank - BTN_BANK_A]);
                return try_enter_sample_card_workflow(dev, (ButtonID)bank);
            }
        }
    }
    return false;
}

bool try_enter_pattern_card_workflow(Device* dev, ButtonID btn) {
    if (!dev || dev->sampling_state != SAMPLING_IDLE || !pattern_mode_active(dev)) return false;
    if (btn >= BTN_BANK_A && btn <= BTN_BANK_D && dev->state.buttons[BTN_CANCEL].pressed) {
        clear_card_management_state(dev);
        clear_bank_lights(dev);
        if (btn <= BTN_BANK_B) {
            if (!backup_slot_has_kind(dev, MEMORY_CARD_BACKUP_PATTERNS)) {
                start_card_emp_lock(dev, true, 0);
                return true;
            }
            dev->sampling_state = SAMPLING_CARD_PATTERN_LOAD_SELECT;
        } else {
            if (get_memory_card_write_protected(dev)) {
                display_raw(dev, SEG_PRT_I[0], SEG_PRT_I[1], SEG_PRT_I[2]);
                return true;
            }
            dev->sampling_state = SAMPLING_CARD_PATTERN_SAVE_SELECT;
        }
        return true;
    }
    if (btn == BTN_CANCEL) {
        for (int bank = BTN_BANK_A; bank <= BTN_BANK_D; ++bank) {
            if (dev->state.buttons[bank].pressed) {
                return try_enter_pattern_card_workflow(dev, (ButtonID)bank);
            }
        }
    }
    return false;
}

int find_empty_pad(Device* dev) {
    int bank_start = dev->state.active_bank * 8;
    for (int i = 0; i < 8; ++i) {
        int pad = bank_start + i;
        if (!dev->pad_has_sample[pad]) return pad;
    }
    return -1;
}

int get_pad_button_id(int pad_index) {
    return BTN_PAD_1 + pad_index;
}

bool is_effect_btn(ButtonID btn) {
    return btn == BTN_FILTER_DRIVE || btn == BTN_PITCH || btn == BTN_DELAY ||
           btn == BTN_MFX || btn == BTN_VINYL_SIM || btn == BTN_ISOLATOR;
}

void set_display_number_plain(Device* dev, int value) {
    int n = std::clamp(value, 0, 999);
    int hundreds = n / 100;
    int tens     = (n % 100) / 10;
    int ones     = n % 10;
    dev->state.display.digit[0] = (hundreds > 0) ? SEG_DIGITS[hundreds] : SEG_BLANK;
    dev->state.display.digit[1] = (n >= 10)      ? SEG_DIGITS[tens]     : SEG_BLANK;
    dev->state.display.digit[2] = SEG_DIGITS[ones];
}

void set_display_measure_beat(Device* dev, int measure, int beat) {
    if (!dev) return;
    int m = std::clamp(measure, 1, 99);
    int b = std::clamp(beat, 1, 4);
    if (m >= 10) {
        dev->state.display.digit[0] = SEG_DIGITS[(m / 10) % 10];
        dev->state.display.digit[1] = SEG_DIGITS[m % 10] | SEG_DP;
        dev->state.display.digit[2] = SEG_DIGITS[b];
    } else {
        dev->state.display.digit[0] = SEG_BLANK;
        dev->state.display.digit[1] = SEG_DIGITS[m] | SEG_DP;
        dev->state.display.digit[2] = SEG_DIGITS[b];
    }
}

void tap_record(uint32_t* times, int* count, uint32_t now) {
    if (!times || !count) return;
    if (*count > 0) {
        uint32_t last = times[*count - 1];
        if ((now - last) > TAP_TIMEOUT_SAMPLES) {
            std::memset(times, 0, sizeof(uint32_t) * 4);
            *count = 0;
        }
    }
    if (*count < 4) {
        times[(*count)++] = now;
    } else {
        times[0] = times[1];
        times[1] = times[2];
        times[2] = times[3];
        times[3] = now;
    }
}

} // namespace sp303
