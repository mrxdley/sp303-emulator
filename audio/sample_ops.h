#pragma once

#include "sp303_audio_internal.h"

void sp303_audio_load_sample_impl(SP303Audio* a, int slot, const float* pcm, uint32_t frames);
void sp303_audio_clear_sample_impl(SP303Audio* a, int slot);
void sp303_audio_swap_samples_impl(SP303Audio* a, int slot_a, int slot_b);
void sp303_audio_set_sample_start_impl(SP303Audio* a, int slot, int value);
void sp303_audio_set_sample_end_impl(SP303Audio* a, int slot, int value);
int  sp303_audio_get_sample_start_impl(SP303Audio* a, int slot);
int  sp303_audio_get_sample_end_impl(SP303Audio* a, int slot);
bool sp303_audio_truncate_sample_impl(SP303Audio* a, int slot);
bool sp303_audio_quantize_sample_end_to_bpm_impl(SP303Audio* a, int slot, int bpm);
void sp303_audio_set_sample_level_impl(SP303Audio* a, int slot, int level);
int  sp303_audio_get_sample_level_impl(SP303Audio* a, int slot);
