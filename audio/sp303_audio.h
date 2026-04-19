#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SP303_AUDIO_QUALITY_STANDARD = 0,
    SP303_AUDIO_QUALITY_LONG,
    SP303_AUDIO_QUALITY_LOFI,
} SP303AudioQuality;

// ─── Constants ────────────────────────────────────────────────────────────────

#define SP303_AUDIO_SLOTS     32   // one per pad (4 banks × 8)
#define SP303_AUDIO_VOICES    8    // polyphony
#define SP303_AUDIO_NAME_LEN  256
#define SP303_AUDIO_MAX_SECONDS 60 // max sample length per pad
#define SP303_AUDIO_MAX_FRAMES (48000 * SP303_AUDIO_MAX_SECONDS) // at 48kHz

// ─── Config ───────────────────────────────────────────────────────────────────

typedef struct {
    char     output_name[SP303_AUDIO_NAME_LEN]; // empty = system default
    char     input_name [SP303_AUDIO_NAME_LEN]; // empty = system default
    uint32_t sample_rate;   // 44100, 48000, 96000 …
    uint32_t buffer_frames; // frames per callback: 128, 256, 512, 1024 …
} SP303AudioConfig;

void sp303_audio_config_default(SP303AudioConfig* cfg);

// ─── Device info ──────────────────────────────────────────────────────────────

typedef struct {
    char name[SP303_AUDIO_NAME_LEN];
    bool is_default;
} SP303AudioDeviceInfo;

// ─── Handle ───────────────────────────────────────────────────────────────────

typedef struct SP303Audio SP303Audio;

SP303Audio* sp303_audio_create (const SP303AudioConfig* cfg); // NULL cfg = defaults
void        sp303_audio_destroy(SP303Audio* a);

// ─── Samples ──────────────────────────────────────────────────────────────────
// slot: 0 = PAD1/BankA … 31 = PAD8/BankD  (same mapping as sp303 button IDs)
// PCM must be mono f32.  sp303_audio holds its own copy.

void sp303_audio_load_sample(SP303Audio* a, int slot,
                             const float* pcm, uint32_t frames);
void sp303_audio_clear_sample(SP303Audio* a, int slot);
void sp303_audio_swap_samples(SP303Audio* a, int slot_a, int slot_b);
void sp303_audio_set_sample_start(SP303Audio* a, int slot, int value); // 0-127
void sp303_audio_set_sample_end  (SP303Audio* a, int slot, int value); // 0-127
int  sp303_audio_get_sample_start(SP303Audio* a, int slot);            // 0-127
int  sp303_audio_get_sample_end  (SP303Audio* a, int slot);            // 0-127
bool sp303_audio_truncate_sample (SP303Audio* a, int slot);            // keep only [start, end)
bool sp303_audio_quantize_sample_end_to_bpm(SP303Audio* a, int slot, int bpm); // bpm: 40-200
void sp303_audio_set_sample_level(SP303Audio* a, int slot, int level); // 0-127
int  sp303_audio_get_sample_level(SP303Audio* a, int slot);            // 0-127

// ─── Playback ─────────────────────────────────────────────────────────────────

void sp303_audio_trigger(SP303Audio* a, int slot); // legacy one-shot trigger
void sp303_audio_trigger_mode(SP303Audio* a, int slot, bool loop, bool gate);
void sp303_audio_note_off    (SP303Audio* a, int slot); // for gate mode release
bool sp303_audio_is_playing  (SP303Audio* a, int slot);
void sp303_audio_stop   (SP303Audio* a, int slot); // silence all voices on slot

// ─── Metering ─────────────────────────────────────────────────────────────────

float sp303_audio_peak(SP303Audio* a); // 0.0 – 1.0+, instantaneous output peak
float sp303_audio_stereo_diff_peak(SP303Audio* a); // 0.0 – 1.0+, |L-R| output peak
void  sp303_audio_set_output_gain(SP303Audio* a, float gain); // 0.0 – 1.0

// ─── Input monitoring ─────────────────────────────────────────────────────────

float sp303_audio_input_peak(SP303Audio* a); // 0.0 – 1.0+, instantaneous input peak
void  sp303_audio_set_input_gain(SP303Audio* a, float gain); // 0.0 – 1.0

// ─── Sampling / Recording ─────────────────────────────────────────────────────
// The audio engine records into an internal ring buffer.
// Call start_recording() to begin, stop_recording() to end.
// The recorded data can then be assigned to a slot.

void sp303_audio_start_recording(SP303Audio* a);
void sp303_audio_stop_recording (SP303Audio* a);
bool sp303_audio_is_recording   (SP303Audio* a);
void sp303_audio_set_recording_mode(SP303Audio* a, bool stereo, SP303AudioQuality quality);

// Returns true if the recording buffer is full (FuL condition)
bool sp303_audio_recording_full(SP303Audio* a);

// Assign the last recording to a slot. Returns false if nothing was recorded.
bool sp303_audio_assign_recording(SP303Audio* a, int slot);

// Discard the current recording buffer.
void sp303_audio_cancel_recording(SP303Audio* a);

// ─── Device enumeration ───────────────────────────────────────────────────────
// Returns count written (≤ max).  Call after create.

int sp303_audio_list_outputs(SP303Audio* a, SP303AudioDeviceInfo* out, int max);
int sp303_audio_list_inputs (SP303Audio* a, SP303AudioDeviceInfo* out, int max);

// ─── Reconfigure ──────────────────────────────────────────────────────────────
// Closes and reopens the audio device.  Samples are preserved.
// Returns false if the new device could not be opened (old device closed).

bool sp303_audio_reconfigure(SP303Audio* a, const SP303AudioConfig* cfg);

#ifdef __cplusplus
}
#endif
