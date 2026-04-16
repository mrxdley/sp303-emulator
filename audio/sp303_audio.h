#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Constants ────────────────────────────────────────────────────────────────

#define SP303_AUDIO_SLOTS     32   // one per pad (4 banks × 8)
#define SP303_AUDIO_VOICES    8    // polyphony
#define SP303_AUDIO_NAME_LEN  256

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

// ─── Playback ─────────────────────────────────────────────────────────────────

void sp303_audio_trigger(SP303Audio* a, int slot); // one-shot; voice-steals if full
void sp303_audio_stop   (SP303Audio* a, int slot); // silence all voices on slot

// ─── Metering ─────────────────────────────────────────────────────────────────

float sp303_audio_peak(SP303Audio* a); // 0.0 – 1.0+, instantaneous output peak

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
