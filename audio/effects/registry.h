#pragma once
#include "effects.h"

namespace sp303 {

// Forward declarations
void filter_drive_init(FxState* s, int sample_rate);
void filter_drive_process(FxState* s, float* io, int frames);

void pitch_init(FxState* s, int sample_rate);
void pitch_process(FxState* s, float* io, int frames);

void delay_init(FxState* s, int sample_rate);
void delay_process(FxState* s, float* io, int frames);

void vinyl_sim_init(FxState* s, int sample_rate);
void vinyl_sim_process(FxState* s, float* io, int frames);

void isolator_init(FxState* s, int sample_rate);
void isolator_process(FxState* s, float* io, int frames);

void reverb_init(FxState* s, int sample_rate);
void reverb_process(FxState* s, float* io, int frames);

void tape_echo_init(FxState* s, int sample_rate);
void tape_echo_process(FxState* s, float* io, int frames);

void chorus_init(FxState* s, int sample_rate);
void chorus_process(FxState* s, float* io, int frames);

void stub_init(FxState* s, int sample_rate);
void stub_process(FxState* s, float* io, int frames);

} // namespace sp303
