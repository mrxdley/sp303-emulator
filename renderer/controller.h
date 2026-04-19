#pragma once

#include "sp303.h"
#include "sp303_audio.h"

#include <vector>

struct RendererController {
    sp303::Audio* audio = nullptr;
    sp303::AudioConfig audio_cfg{};
    float peak_threshold = 0.05f;

    std::vector<sp303::AudioDeviceInfo> out_devs;
    std::vector<sp303::AudioDeviceInfo> in_devs;
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
    bool was_time_bpm_mode = false;
    int time_bpm_display_kind = 0; // 0=number, 1=off, 2=pattern
    int time_bpm_display_value = 120;
};

bool renderer_controller_init(RendererController* c);
void renderer_controller_shutdown(RendererController* c);
sp303::State renderer_controller_step(RendererController* c, sp303::Device* dev, int active_knob);
void renderer_controller_refresh_devices(RendererController* c);
bool renderer_controller_apply_audio_config(RendererController* c);
