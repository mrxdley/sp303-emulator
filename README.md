# SP-303 Emulator

Roland SP-303-inspired sampler emulator in C++.

This project is not a generic beat pad app. It is an in-progress attempt to reproduce SP-303 workflows, states, button chords, display behavior, and sample-edit logic closely enough to be tested against the original manual.

## Current State

Implemented in some form:
- sampling
- threshold sampling mode
- resampling
- sample fine edit: `START / END / LEVEL`
- `MARK`-based start/end workflows
- sample BPM correction and time modify
- loop / one-shot / gate / trigger / reverse playback
- pad/sample swap, delete, truncate, delete-all
- per-pad effects routing
- direct effects:
  - `FILTER+DRIVE`
  - `PITCH`
  - `DELAY`
  - `VINYL SIM`
  - `ISOLATOR`
- MFX currently implemented:
  - `1 REVERB`
  - `2 TAPE ECHO`
  - `3 CHORUS`
  - `4 FLANGER`
  - `5 PHASER`
  - `6 TREMOLO/PAN`
- pattern sequencer core workflows
- project quick save/load
- virtual memory card model
- sample/pattern backup save/load through the card system
- pattern-event velocity persistence in the engine

Still rough / incomplete:
- many edge cases still need manual cross-checking
- remaining MFX `7..21`
- renderer skinning / final visual design
- MIDI input
- import workflow for external WAV/AIFF files
- some manual-faithful behaviors are still under investigation

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run:

```bash
./build/sp303_renderer
```

Smoke test:

```bash
./build/sp303_test
```

## Dependencies

Fetched automatically by CMake:
- `raylib`
- `nlohmann/json`
- `miniaudio`

## Repo Layout

```text
core/
  SP-303 device state machine, workflows, display/LED state, pattern logic,
  memory-card state, renderer-facing state snapshot

audio/
  sample storage, playback, recording/resampling, effect DSP, voice handling

renderer/
  raylib frontend, controller bridge, project I/O, card image I/O

tests/
  smoke test
```

Key files:
- [core/sp303.cpp](core/sp303.cpp)
- [core/sp303_card.cpp](core/sp303_card.cpp)
- [core/sp303_state.cpp](core/sp303_state.cpp)
- [audio/sp303_audio.cpp](audio/sp303_audio.cpp)
- [audio/effect.cpp](audio/effect.cpp)
- [renderer/main.cpp](renderer/main.cpp)
- [renderer/controller.cpp](renderer/controller.cpp)
- [renderer/card_io.cpp](renderer/card_io.cpp)

## Virtual Memory Card

This emulator uses a virtual SmartMedia-style card image backed by a folder on disk.

Current model:
- banks `A/B` = internal memory
- banks `C/D` = live card memory
- card backup slots `1..8` store internal sample/pattern sets

The active card folder is selected from the renderer audio/config tab. Card state is serialized to a folder containing:
- `card.json`
- sample WAV files

## Documentation

Start here:
- [docs/INDEX.md](docs/INDEX.md)

Deep docs already in repo:
- [ONBOARDING.md](ONBOARDING.md)
- [PROJECT_DOCUMENTATION.md](PROJECT_DOCUMENTATION.md)
- [MANUAL_TEST_SUITE.md](MANUAL_TEST_SUITE.md)
- [TODO.md](TODO.md)
- [PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md](PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md)
- [HOW-TO-SAVE-LOAD-IMPLEMENT.md](HOW-TO-SAVE-LOAD-IMPLEMENT.md)

## Design Intent

A lot of behavior here is intentionally awkward because the hardware is awkward.

When in doubt:
- prefer the SP-303 manual over “clean app” instincts
- prefer explicit state transitions over clever hidden behavior
- test workflows as button sequences, not isolated helper functions

## Publishing Notes

Before public screenshots or demos:
- assume the renderer is still an engineering UI, not a finished skin
- verify manual workflows against [MANUAL_TEST_SUITE.md](MANUAL_TEST_SUITE.md)
- decide what should be described as authentic behavior vs emulator convenience
