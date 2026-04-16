#include "sp303.h"
#include <cstdio>
#include <cassert>

static void print_display(const SP303Display& d) {
    std::printf("display: [0x%02X][0x%02X][0x%02X]\n",
        d.digit[0], d.digit[1], d.digit[2]);
}

int main() {
    SP303Device* dev = sp303_create();
    assert(dev);

    // Print all button labels
    std::printf("--- Buttons ---\n");
    for (int i = 0; i < SP303_BTN_COUNT; ++i) {
        const auto& def = SP303_BUTTON_DEFS[i];
        std::printf("  [%02d] %-20s  alt1: %-14s  alt2: %s\n",
            i,
            def.primary,
            def.alt1 ? def.alt1 : "-",
            def.alt2 ? def.alt2 : "-");
    }

    // Print all knob labels
    std::printf("\n--- Knobs ---\n");
    for (int i = 0; i < SP303_KNOB_COUNT; ++i) {
        const auto& def = SP303_KNOB_DEFS[i];
        std::printf("  [%d] %-12s  alt1: %-8s  alt2: %s\n",
            i,
            def.primary,
            def.alt1 ? def.alt1 : "-",
            def.alt2 ? def.alt2 : "-");
    }

    // Button press / LED state
    sp303_button_down(dev, SP303_BTN_PAD_1);
    sp303_button_down(dev, SP303_BTN_BANK_A);
    SP303State s = sp303_get_state(dev);
    assert(s.buttons[SP303_BTN_PAD_1].pressed  == true);
    assert(s.buttons[SP303_BTN_BANK_A].pressed == true);
    assert(s.buttons[SP303_BTN_PAD_2].pressed  == false);

    sp303_button_up(dev, SP303_BTN_PAD_1);
    s = sp303_get_state(dev);
    assert(s.buttons[SP303_BTN_PAD_1].pressed == false);

    // Knob clamping
    sp303_knob_set(dev, SP303_KNOB_VOLUME, 2.5f);
    s = sp303_get_state(dev);
    assert(s.knobs[SP303_KNOB_VOLUME].value == 1.0f);

    sp303_knob_set(dev, SP303_KNOB_CUTOFF, 0.75f);
    s = sp303_get_state(dev);
    assert(s.knobs[SP303_KNOB_CUTOFF].value == 0.75f);

    // Display: number
    std::printf("\n--- Display ---\n");
    sp303_display_number(dev, 0);   s = sp303_get_state(dev); print_display(s.display);
    sp303_display_number(dev, 42);  s = sp303_get_state(dev); print_display(s.display);
    sp303_display_number(dev, 303); s = sp303_get_state(dev); print_display(s.display);
    sp303_display_number(dev, 999); s = sp303_get_state(dev); print_display(s.display);

    // Display: "Err"
    sp303_display_raw(dev, SP303_SEG_ERR[0], SP303_SEG_ERR[1], SP303_SEG_ERR[2]);
    s = sp303_get_state(dev);
    std::printf("Err: "); print_display(s.display);

    // Display: "---"
    sp303_display_raw(dev, SP303_SEG_DASH, SP303_SEG_DASH, SP303_SEG_DASH);
    s = sp303_get_state(dev);
    std::printf("---: "); print_display(s.display);

    // Indicators
    sp303_indicator_set(dev, SP303_IND_PEAK, true);
    s = sp303_get_state(dev);
    assert(s.indicators[SP303_IND_PEAK].lit == true);
    sp303_indicator_set(dev, SP303_IND_PEAK, false);
    s = sp303_get_state(dev);
    assert(s.indicators[SP303_IND_PEAK].lit == false);

    // Tick
    sp303_tick(dev, 44100);

    sp303_destroy(dev);
    std::printf("\nAll assertions passed.\n");
    return 0;
}
