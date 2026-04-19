#pragma once
#include <stdint.h>
#include <vector>

namespace sp303 {

enum AudioQuality {
    AUDIO_QUALITY_STANDARD = 0,
    AUDIO_QUALITY_LONG,
    AUDIO_QUALITY_LOFI,
};

// ─── Constants ────────────────────────────────────────────────────────────────

constexpr int AUDIO_SLOTS       = 32;   // one per pad (4 banks × 8)
constexpr int AUDIO_VOICES      = 8;    // polyphony
constexpr int AUDIO_NAME_LEN    = 256;
constexpr int AUDIO_MAX_SECONDS = 60;   // max sample length per pad
constexpr int AUDIO_MAX_FRAMES  = 48000 * AUDIO_MAX_SECONDS; // at 48kHz

// ─── Config ───────────────────────────────────────────────────────────────────

struct AudioConfig {
    char     output_name[AUDIO_NAME_LEN]; // empty = system default
    char     input_name [AUDIO_NAME_LEN]; // empty = system default
    uint32_t sample_rate;   // 44100, 48000, 96000 …
    uint32_t buffer_frames; // frames per callback: 128, 256, 512, 1024 …
};

void audio_config_default(AudioConfig* cfg);

// ─── Device info ──────────────────────────────────────────────────────────────

struct AudioDeviceInfo {
    char name[AUDIO_NAME_LEN];
    bool is_default;
};

// ─── Handle ───────────────────────────────────────────────────────────────────

struct Audio;

Audio* audio_create (const AudioConfig* cfg); // nullptr cfg = defaults
void   audio_destroy(Audio* a);

// ─── Samples ──────────────────────────────────────────────────────────────────
// slot: 0 = PAD1/BankA … 31 = PAD8/BankD  (same mapping as button IDs)
// PCM must be mono f32.  Audio holds its own copy.

void audio_load_sample(Audio* a, int slot, const float* pcm, uint32_t frames);
void audio_clear_sample(Audio* a, int slot);
void audio_swap_samples(Audio* a, int slot_a, int slot_b);
bool audio_has_sample(Audio* a, int slot);
bool audio_export_sample(Audio* a, int slot, std::vector<float>* pcm, uint32_t* channels, uint32_t* frames);
bool audio_import_sample(Audio* a, int slot, const float* pcm, uint32_t frames, uint32_t channels);
void audio_set_sample_start(Audio* a, int slot, int value); // 0-127
void audio_set_sample_end  (Audio* a, int slot, int value); // 0-127
int  audio_get_sample_start(Audio* a, int slot);            // 0-127
int  audio_get_sample_end  (Audio* a, int slot);            // 0-127
bool audio_truncate_sample (Audio* a, int slot);            // keep only [start, end)
bool audio_quantize_sample_end_to_bpm(Audio* a, int slot, int bpm); // bpm: 40-200
void audio_set_sample_level(Audio* a, int slot, int level); // 0-127
int  audio_get_sample_level(Audio* a, int slot);            // 0-127
int  audio_get_sample_bpm(Audio* a, int slot);              // 40-200 normalized
void audio_set_sample_bpm_adjust(Audio* a, int slot, int adjust); // -1,0,+1
int  audio_get_sample_bpm_adjust(Audio* a, int slot);       // -1,0,+1
void audio_set_sample_time_mode(Audio* a, int slot, int mode, int target_bpm); // mode 0=off 1=custom 2=ptn
int  audio_get_sample_time_mode(Audio* a, int slot);        // 0=off 1=custom 2=ptn
int  audio_get_sample_time_target_bpm(Audio* a, int slot);  // custom bpm or -1
void audio_set_pattern_bpm(Audio* a, int bpm);              // 40-200
int  audio_get_pattern_bpm(Audio* a);                       // 40-200
void audio_trigger_metronome(Audio* a, int level, bool accent);
int  audio_get_sample_playhead(Audio* a, int slot);         // 0-127 absolute within sample
int  audio_get_pad_led_hold_frames(Audio* a, int slot, bool reverse); // playback length in UI frames + 3s tail

// ─── Playback ─────────────────────────────────────────────────────────────────

void audio_trigger(Audio* a, int slot); // legacy one-shot trigger
void audio_trigger_mode(Audio* a, int slot, bool loop, bool gate, bool reverse);
void audio_note_off    (Audio* a, int slot); // for gate mode release
bool audio_is_playing  (Audio* a, int slot);
void audio_stop        (Audio* a, int slot); // silence all voices on slot

// ─── Metering ─────────────────────────────────────────────────────────────────

float audio_peak(Audio* a); // 0.0 – 1.0+, instantaneous output peak
float audio_stereo_diff_peak(Audio* a); // 0.0 – 1.0+, |L-R| output peak
void  audio_set_output_gain(Audio* a, float gain); // 0.0 – 1.0

// ─── Input monitoring ─────────────────────────────────────────────────────────

float audio_input_peak(Audio* a); // 0.0 – 1.0+, instantaneous input peak
void  audio_set_input_gain(Audio* a, float gain); // 0.0 – 1.0

// ─── Sampling / Recording ─────────────────────────────────────────────────────
// The audio engine records into an internal ring buffer.
// Call start_recording() to begin, stop_recording() to end.
// The recorded data can then be assigned to a slot.

void audio_start_recording(Audio* a);
void audio_stop_recording (Audio* a);
bool audio_is_recording   (Audio* a);
void audio_set_recording_mode(Audio* a, bool stereo, AudioQuality quality);
void audio_set_record_from_output(Audio* a, bool enabled);

// Returns true if the recording buffer is full (FuL condition)
bool audio_recording_full(Audio* a);

// Assign the last recording to a slot. Returns false if nothing was recorded.
bool audio_assign_recording(Audio* a, int slot);

// Discard the current recording buffer.
void audio_cancel_recording(Audio* a);

// ─── Device enumeration ───────────────────────────────────────────────────────
// Returns count written (≤ max).  Call after create.

int audio_list_outputs(Audio* a, AudioDeviceInfo* out, int max);
int audio_list_inputs (Audio* a, AudioDeviceInfo* out, int max);

// ─── Reconfigure ──────────────────────────────────────────────────────────────
// Closes and reopens the audio device.  Samples are preserved.
// Returns false if the new device could not be opened (old device closed).

bool audio_reconfigure(Audio* a, const AudioConfig* cfg);

// ─── Effect routing ───────────────────────────────────────────────────────────
// Call every frame from the controller thread (not the audio callback).
// active_btn: ButtonID of the globally active effect, or -1 for none.
// mfx_type:  0-20 (MFX sub-type selector; ignored for other effects).
// pad_mask:  bitmask — bit i = pad i carries the active effect.

void audio_set_effect_routing(Audio* a, int active_btn, int mfx_type, uint32_t pad_mask);
void audio_set_effect_params (Audio* a, float p1, float p2, float p3);

} // namespace sp303
