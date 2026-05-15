# SP-303

Workflow-central SP-303 emulator in C++.

This is not a generic sampler app with SP-303 styling on top. The project is built around reproducing the machine's actual interaction model: button chords, modal states, display behavior, awkward transitions, pattern flow, resampling habits, and memory-card workflows.

If you know the hardware, you know this software.

<img width="501" height="703" alt="image" src="https://github.com/user-attachments/assets/3c1063e9-6a30-4357-8255-37870afe7447" />

## What It Does

Implemented now:
- sampling
- threshold sampling
- resampling
- `START / END / LEVEL` sample editing
- `MARK`-based trim workflows
- sample BPM correction and time modify
- loop / one-shot / gate / reverse playback
- hold-latched gate playback
- pad swap, delete, truncate, delete-all
- per-pad effect routing
- direct effects:
  - `FILTER+DRIVE`
  - `PITCH`
  - `DELAY`
  - `VINYL SIM`
  - `ISOLATOR`
- MFX implemented so far:
  - `1 REVERB`
  - `2 TAPE ECHO`
  - `3 CHORUS`
  - `4 FLANGER`
  - `5 PHASER`
  - `6 TREMOLO/PAN`
- pattern recording / playback
- pattern velocity persistence
- pattern hold events
- project quicksave / quickload
- virtual memory card model
- sample / pattern backup save-load through card workflows

Still open:
- remaining MFX `7..21`
- MIDI input
- more manual-faithfulness cleanup
- renderer / presentation polish
- some DSP tuning and edge-case verification

  <img width="446" height="421" alt="image" src="https://github.com/user-attachments/assets/f34dba52-3f6e-45d6-96ce-0aca3aac71cb" />

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the renderer:

```bash
./build/sp303_renderer
```

Run the smoke test:

```bash
./build/sp303_test
```

## Project Layout

```text
core/
  device state machine, button logic, display state, pattern logic,
  memory-card state, renderer-facing queries

audio/
  sample storage, playback, recording/resampling, effects, voice handling

renderer/
  raylib frontend, input bridge, project I/O, card I/O, layout logic

tests/
  smoke coverage
```

Important files:
- [core/sp303.cpp](core/sp303.cpp)
- [core/sp303_card.cpp](core/sp303_card.cpp)
- [core/sp303_state.cpp](core/sp303_state.cpp)
- [audio/sp303_audio.cpp](audio/sp303_audio.cpp)
- [audio/effect.cpp](audio/effect.cpp)
- [renderer/main.cpp](renderer/main.cpp)
- [renderer/controller.cpp](renderer/controller.cpp)
- [renderer/card_io.cpp](renderer/card_io.cpp)

## Virtual Memory Card

The emulator uses a SmartMedia-style virtual card backed by a folder on disk.

Current model:
- banks `A/B` are internal
- banks `C/D` are live card memory
- backup slots `1..8` store internal sample / pattern sets

The active card folder is selected from the UI. Card contents are serialized as:
- `card.json`
- sample WAV files

## Documentation

Start here:
- [docs/INDEX.md](docs/INDEX.md)

Useful references in the repo:
- [ONBOARDING.md](ONBOARDING.md)
- [PROJECT_DOCUMENTATION.md](PROJECT_DOCUMENTATION.md)
- [MANUAL_TEST_SUITE.md](MANUAL_TEST_SUITE.md)
- [TODO.md](TODO.md)
- [docs/AUDIO_LOOPBACK_HOWTO.md](docs/AUDIO_LOOPBACK_HOWTO.md)
- [docs/KNOWN_INACCURACIES.md](docs/KNOWN_INACCURACIES.md)
- [PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md](PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md)
- [HOW-TO-SAVE-LOAD-IMPLEMENT.md](HOW-TO-SAVE-LOAD-IMPLEMENT.md)

## Design Notes

The project does not try to smooth out every rough edge in the original workflow.

When there is a choice between:
- a cleaner modern UX
- and a more recognisable SP-303 interaction

the second option usually wins.

That also means some awkward behavior is deliberate.

## Status

This is already usable as an instrument, with more updates on the way.

It is not finished, and it is not claiming perfect hardware fidelity yet. But the core workflow is there, and the remaining work is mostly accuracy, coverage, and polish rather than basic capability.
