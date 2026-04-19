#include "sp303.h"
#include <cstdio>
#include <cassert>

static void print_display(const sp303::Display& d) {
    std::printf("display: [0x%02X][0x%02X][0x%02X]\n",
        d.digit[0], d.digit[1], d.digit[2]);
}

int main() {
    sp303::Device* dev = sp303::create();
    assert(dev);

    // Print all button labels
    std::printf("--- Buttons ---\n");
    for (int i = 0; i < sp303::BTN_COUNT; ++i) {
        const auto& def = sp303::BUTTON_DEFS[i];
        std::printf("  [%02d] %-20s  alt1: %-14s  alt2: %s\n",
            i,
            def.primary,
            def.alt1 ? def.alt1 : "-",
            def.alt2 ? def.alt2 : "-");
    }

    // Print all knob labels
    std::printf("\n--- Knobs ---\n");
    for (int i = 0; i < sp303::KNOB_COUNT; ++i) {
        const auto& def = sp303::KNOB_DEFS[i];
        std::printf("  [%d] %-12s  alt1: %-8s  alt2: %s\n",
            i,
            def.primary,
            def.alt1 ? def.alt1 : "-",
            def.alt2 ? def.alt2 : "-");
    }

    // Button press / LED state
    sp303::button_down(dev, sp303::BTN_PAD_1);
    sp303::button_down(dev, sp303::BTN_BANK_A);
    sp303::State s = sp303::get_state(dev);
    assert(s.buttons[sp303::BTN_PAD_1].pressed  == true);
    assert(s.buttons[sp303::BTN_BANK_A].pressed == true);
    assert(s.buttons[sp303::BTN_PAD_2].pressed  == false);

    sp303::button_up(dev, sp303::BTN_PAD_1);
    s = sp303::get_state(dev);
    assert(s.buttons[sp303::BTN_PAD_1].pressed == false);

    // Knob clamping
    sp303::knob_set(dev, sp303::KNOB_VOLUME, 2.5f);
    s = sp303::get_state(dev);
    assert(s.knobs[sp303::KNOB_VOLUME].value == 1.0f);

    sp303::knob_set(dev, sp303::KNOB_CUTOFF, 0.75f);
    s = sp303::get_state(dev);
    assert(s.knobs[sp303::KNOB_CUTOFF].value == 0.75f);

    // Display: number
    std::printf("\n--- Display ---\n");
    sp303::display_number(dev, 0);   s = sp303::get_state(dev); print_display(s.display);
    sp303::display_number(dev, 42);  s = sp303::get_state(dev); print_display(s.display);
    sp303::display_number(dev, 303); s = sp303::get_state(dev); print_display(s.display);
    sp303::display_number(dev, 999); s = sp303::get_state(dev); print_display(s.display);

    // Display: "Err"
    sp303::display_raw(dev, sp303::SEG_ERR[0], sp303::SEG_ERR[1], sp303::SEG_ERR[2]);
    s = sp303::get_state(dev);
    std::printf("Err: "); print_display(s.display);

    // Display: "---"
    sp303::display_raw(dev, sp303::SEG_DASH, sp303::SEG_DASH, sp303::SEG_DASH);
    s = sp303::get_state(dev);
    std::printf("---: "); print_display(s.display);

    // Indicators
    sp303::indicator_set(dev, sp303::IND_PEAK, true);
    s = sp303::get_state(dev);
    assert(s.indicators[sp303::IND_PEAK].lit == true);
    sp303::indicator_set(dev, sp303::IND_PEAK, false);
    s = sp303::get_state(dev);
    assert(s.indicators[sp303::IND_PEAK].lit == false);

    // Tick
    sp303::tick(dev, 44100);

    sp303::destroy(dev);
    std::printf("\nAll assertions passed.\n");
    return 0;
}
