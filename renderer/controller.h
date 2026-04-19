#pragma once

#include "sp303.h"
#include "sp303_audio.h"

#include <vector>

struct RendererController {
    SP303Audio* audio = nullptr;
    SP303AudioConfig audio_cfg{};
    float peak_threshold = 0.05f;

    std::vector<SP303AudioDeviceInfo> out_devs;
    std::vector<SP303AudioDeviceInfo> in_devs;
    int sel_out = 0;
    int sel_in = 0;
    int sel_rate = 0;
    int sel_buf = 2;
    bool playback_ok = false;

    float config_input_peak = 0.0f;
    bool stereo_activity_lit = false;
    int peak_hold_frames = 0;
    int sample_silence_frames = 0;
    bool was_editing_sample = false;
};

bool renderer_controller_init(RendererController* c);
void renderer_controller_shutdown(RendererController* c);
SP303State renderer_controller_step(RendererController* c, SP303Device* dev, int active_knob);
void renderer_controller_refresh_devices(RendererController* c);
bool renderer_controller_apply_audio_config(RendererController* c);
