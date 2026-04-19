#pragma once

#include "sp303_audio_internal.h"

namespace sp303 {

void audio_load_sample_impl(Audio* a, int slot, const float* pcm, uint32_t frames);
void audio_clear_sample_impl(Audio* a, int slot);
void audio_swap_samples_impl(Audio* a, int slot_a, int slot_b);
void audio_set_sample_start_impl(Audio* a, int slot, int value);
void audio_set_sample_end_impl(Audio* a, int slot, int value);
int  audio_get_sample_start_impl(Audio* a, int slot);
int  audio_get_sample_end_impl(Audio* a, int slot);
bool audio_truncate_sample_impl(Audio* a, int slot);
bool audio_quantize_sample_end_to_bpm_impl(Audio* a, int slot, int bpm);
void audio_set_sample_level_impl(Audio* a, int slot, int level);
int  audio_get_sample_level_impl(Audio* a, int slot);
int  audio_get_sample_bpm_impl(Audio* a, int slot);
void audio_set_sample_bpm_adjust_impl(Audio* a, int slot, int adjust);
int  audio_get_sample_bpm_adjust_impl(Audio* a, int slot);
void audio_set_sample_time_mode_impl(Audio* a, int slot, int mode, int target_bpm);
int  audio_get_sample_time_mode_impl(Audio* a, int slot);
int  audio_get_sample_time_target_bpm_impl(Audio* a, int slot);

} // namespace sp303
