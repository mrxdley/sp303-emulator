# Save / Load Implementation Notes

This is a future feature note. Do **not** implement save/load yet.

Reason:
- Save/load should come **after** pattern sequencing is implemented.
- A useful save format needs to include both sample state and pattern data, otherwise we will have to redesign it immediately afterwards.

## Agreed constraints

- Start/end do **not** need frame-accurate persistence.
- It is acceptable to persist sample trim using the existing `0..127` representation.
- Small drift from re-mapping `0..127` back onto waveform length is acceptable.
- This project does **not** need a perfect archival format. It needs a practical project format.

## What save/load should cover

When this gets implemented, it should save:
- all 32 pad sample assignments
- per-pad sample audio data
- per-pad level
- per-pad start value `0..127`
- per-pad end value `0..127`
- per-pad loop mode
- per-pad gate mode
- per-pad reverse mode
- per-pad BPM
- per-pad time-modify mode
- per-pad time-modify ratio / state
- global sample trigger threshold
- global sampling options that matter to project behavior
- pattern sequencing data once sequencing exists

It should not save transient runtime state such as:
- blinking LEDs
- currently pressed buttons
- active playback voices
- current edit mode
- temporary display contents
- current capture buffer

## Recommended on-disk layout

Use a project folder, not one monolithic blob:

```text
projects/my_project/
  project.json
  samples/
    A1.wav
    A2.wav
    B5.wav
```

Why:
- sample audio should live as normal files
- metadata should stay human-readable
- debugging broken projects becomes easy
- replacing one sample manually is trivial

## Recommended format

Use:
- `project.json` for metadata
- `.wav` files for pad sample audio

For each pad, store metadata like:
- `pad`
- `has_sample`
- `sample_file`
- `level`
- `start_127`
- `end_127`
- `loop`
- `gate`
- `reverse`
- `bpm`
- `time_mode`
- `time_ratio`

Once pattern sequencing exists, add:
- pattern bank data
- pattern tempo data
- step/event data
- any sequencing-related per-pad playback flags that affect playback

## Recommended code structure

When this is implemented, keep it out of `main.cpp`.

Add a dedicated module:
- `project_io.h`
- `project_io.cpp`

Responsibilities:
- save project metadata
- load project metadata
- write sample WAV files
- read sample WAV files
- validate project version
- handle missing/corrupt files gracefully

## Audio-side requirements

Current audio state already contains most of the sample data needed:
- PCM buffer
- channel count
- level
- start/end frames

When saving:
- export audio as WAV per occupied pad
- save trim metadata as `0..127`, not exact frames

When loading:
- load WAV into the target pad
- restore level
- restore trim using the existing `audio_set_sample_start(...)` and `audio_set_sample_end(...)` style APIs

Because we already accept `0..127` persistence, we do **not** need exact-frame save/load APIs unless a later feature makes them necessary.

## Core-side requirements

Core owns pad behavior state that also needs persistence:
- whether pad is occupied
- loop / gate / reverse
- BPM
- time-modify settings
- threshold-related settings if they are project-level

This means save/load needs explicit core export/import helpers instead of reaching directly into internals from renderer code.

Recommended future API shape:
- `export_pad_state(pad)`
- `import_pad_state(pad, data)`
- `save_project(path)`
- `load_project(path)`

## Load behavior

When loading a project:
1. Parse `project.json`.
2. Clear the current project state.
3. Load each sample WAV into its pad.
4. Restore per-pad behavior flags and values.
5. Restore sequencing data once that exists.
6. Reset runtime/transient state back to a clean idle state.

After load, the machine should behave like:
- display shows `---`
- no edit mode active
- no pending delete/swap/truncate action
- no stuck playback state

## Save behavior

When saving a project:
1. Create a temporary project directory.
2. Write sample WAVs.
3. Write `project.json`.
4. Rename temp directory into place.

This avoids leaving a half-written project behind if the save fails midway.

## Versioning

Add a simple project version integer early.

Example:

```json
{
  "version": 1
}
```

This matters because sequencing will almost certainly expand the file format.

## What to do later, after sequencing

When pattern sequencing is in place, revisit this file and decide:
- exact structure for pattern data
- whether master volume should persist
- whether audio device config should persist in project or stay app-global
- whether banks C/D should later map to a card-style storage model

## Short conclusion

The correct order is:
1. finish pattern sequencing
2. define full project data model
3. implement project save/load on top of that

Do not build save/load before sequencing unless there is a temporary debug-only reason.
