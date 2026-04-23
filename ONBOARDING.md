# SP-303 Emulator Onboarding

This repo is an in-progress Roland SP-303-style sampler emulator. Treat it as a hardware behavior simulation, not a generic sampler UI. The manual-driven workflows matter.

## First Rules

- Do not casually rewrite workflows. Most weird-looking behavior is trying to match the SP-303 manual.
- Do not commit unless the user explicitly asks.
- The worktree may be dirty. Never revert unrelated changes.
- Build after code edits with:
  ```bash
  cmake --build build
  ```
- Main executable:
  ```bash
  ./build/sp303_renderer
  ```

## Top-Level Map

- `core/`
  - Device state machine and renderer-facing state snapshot.
  - Owns button state, LEDs, display state, banks, pattern UI state, sample workflow state, memory-card workflow state.
  - Current split:
    - `core/sp303.cpp`
      - main state machine, lifecycle, input handling, tick, display helpers that are still state-machine-adjacent, and public queries
    - `core/sp303_card.cpp`
      - card workflow helpers, bank/pad helper utilities, pattern/card save-load workflow entry logic
    - `core/sp303_state.cpp`
      - pattern export/import helpers, pad project state, memory-card backup state accessors, project reset helpers
    - `core/sp303_internal.h`
      - private `Device` layout and internal helper declarations shared across the split core files

- `audio/`
  - miniaudio playback/capture, sample storage, voices, effects, recording buffers.
  - Important files: `audio/sp303_audio.cpp`, `audio/effect.cpp`, `audio/effect.h`, `audio/effects/*.cpp`

- `renderer/`
  - raylib UI, mouse/keyboard input, project/card IO, controller bridge between core and audio.
  - Renderer should stay dumb where possible. Prefer putting device behavior in `core`.
  - Important files: `renderer/main.cpp`, `renderer/controller.cpp`, `renderer/project_io.cpp`, `renderer/card_io.cpp`

- `MANUAL_TEST_SUITE.md`
  - Manual-driven test checklist. Update it when workflows change.

- `VELOCITY_MANUAL_TESTS.md`
  - Velocity-specific manual verification plan.
  - Covers attenuation-only behavior, pattern persistence, backward compatibility, and future MIDI use.

- `TODO.md`
  - Live notes, known inaccuracies, convenience ideas, and implementation gaps.

## Device Concepts

- There are 32 sample pads: banks A-D, 8 pads each.
- Banks A/B are internal memory. Banks C/D represent the virtual memory card.
- `State` in `core/sp303.h` is the renderer snapshot: LEDs, display, active bank, knobs, indicators.
- Display is 3 digits, 7-segment encoded.
- Pads generally use `pad_led_hold_frames[]` for temporary lighting.
- The private `Device` struct is no longer only in `core/sp303.cpp`; it lives in `core/sp303_internal.h`.

## Sampling

Basic flow:

1. `REC` in idle enters standby.
2. Empty pads blink.
3. Press a destination pad.
4. `REC` arms ready mode.
5. Audio threshold starts actual recording unless resampling.
6. `REC` stops recording.
7. Recording assigns to the target pad.

Threshold mode:

- `CANCEL + REC` enters sample threshold mode.
- `CTRL 3` changes threshold `0..8`.
- Display shows `-n-`.

Sampling quality:

- Default is mono.
- `STEREO` lit means stereo sampling.
- `LONG/LO-FI` cycles standard/long/lofi where valid.
- In idle, `LONG/LO-FI` should not stay lit.

## Resampling

Current intended flow:

1. Press `RESAMPLE`: lit, display `LEV`.
2. `CTRL 3` controls resampling gain and displays `0..127` once moved.
3. Press `REC`: destination select, empty pads blink.
4. Pick destination pad.
5. Press `REC`: armed, filled pads blink.
6. Play one or more source pads.
7. Press `REC` to stop. No silence autostop.

Important: resampling records output, not capture input.

## Sample Edit: START/END/LEVEL

- Last played pad is the edit target.
- Press `START/END/LEVEL`: display `Edt`.
- `CTRL 1` = start, `CTRL 2` = end, `CTRL 3` = level.
- Values are `0..127`.
- Display should persist on the last changed value until exit.
- Pressing the same last-played pad can retrigger without exiting.
- Pressing another pad exits edit mode.

## MARK

Manual behaviors implemented:

- Press `MARK` while a sample is playing: set/queue start point.
- Press `MARK` again while playing: set end point.
- Hold `MARK` and press a pad: enter end-only mode for that playback.
- Press lit `MARK` while sample is playing: reset full range.
- Pressing `MARK` with no pad audio playing should do nothing.

Known trap: do not let held-MARK pad press also enter the normal both-start-and-end path.

## Pattern Sequencer

Pattern mode is active when `PATTERN SELECT` is lit or a pattern is playing.

Basic flow:

1. `PATTERN SELECT`: display `Ptn`, pads with patterns blink.
2. `REC`: record-select, empty pattern pads blink, metronome preview starts.
3. Pick pattern pad: selected pad solid, `REC` blinks.
4. Optional setup:
   - `TIME/BPM` + `CTRL 2`: global pattern BPM.
   - `START/END/LEVEL` + `CTRL 3`: metronome level.
   - `LENGTH` + `CTRL 3`: pattern length.
   - `QUANTIZE` + `CTRL 3`: quantize value.
5. `REC`: one-measure count-in, then recording/overdub.
6. `REC`: stop recording.

Playback:

- Press pattern pad to play.
- `CANCEL` stops patterns.
- When pattern UI is open, pattern pads show pattern slots:
  - patterns blink
  - currently playing pattern is solid
- When pattern UI is not open and a pattern is playing, pad LEDs should show sample pad hits generated by the sequencer.

Velocity:

- Pattern events now store velocity as part of the event data.
- Current keyboard/mouse playback still records fixed velocity `127`.
- Future MIDI input should feed real per-hit velocity into the same path.
- Velocity is attenuation-only:
  - `127` preserves the current sample level
  - lower values only reduce from that level
  - velocity must never boost a pad above its current `LEVEL` setting

Known nuance:

- Sample-pad lighting during patterns is currently based on trigger/hold timing, not true sequencer note-off duration. If implementing held durations, extend `PatternEvent` to include note length/on-off.

## Effects

Dedicated effects:

- `FILTER+DRIVE`
- `PITCH`
- `DELAY`
- `VINYL SIM`
- `ISOLATOR`

MFX:

- MFX behaves like other effect buttons for assignment.
- Hold `MFX` and move `CTRL 3` to select `mfx_type` `1..21`.
- Implemented MFX DSP:
  - `1 = REVERB`
  - `2 = TAPE ECHO`
  - `3 = CHORUS`
  - `4 = FLANGER`
  - `5 = PHASER`
  - `6 = TREMOLO/PAN`
- `7..21` still have no DSP yet.
- Do not alias dedicated effects like delay/pitch into MFX.
- Add real MFX algorithms as separate files later.

Effect architecture:

- `audio/effect.h` defines `EffectDef`.
- Insert effects use `process_frame`.
- Bus effects use `process_buffer`.
- `audio/effect.cpp` maps buttons/subtypes to effect registry indices.
- Bus effect state must reset when active effect or MFX subtype changes.

Pitch note:

- Current `PITCH` uses playback-speed change, not hardware-style pitch shifting.
- Pitch down can feel like sample length behavior is wrong. Audit cursor/end handling before changing it.

Velocity note:

- Engine-side voice velocity exists now in `audio/sp303_audio.cpp`.
- The trigger path supports per-voice velocity, but current renderer input still uses fixed full velocity.
- Pattern persistence and card/project persistence already preserve velocity values.

## Save/Load And Memory Card

There are two distinct persistence ideas:

- Project quicksave/load:
  - developer convenience for saving the whole emulator state.
  - F5/F9 in renderer.

- Virtual memory card:
  - represented by folders.
  - card path configurable in TAB config.
  - backs memory-card style save/load workflows.

Memory-card workflows are manual-inspired and easy to regress. Be careful with:

- `CANCEL + A/B`: sample or pattern load from backup area.
- `CANCEL + C/D`: sample or pattern save to backup area.
- Empty loads should show `EMP` for 3 seconds and lock input.
- During `EMP`, bank pair A/B or C/D lights together.
- Pattern save/load now also includes per-event velocity.

## Renderer Input Notes

- Mouse and keyboard both call the same core button APIs.
- `pressed_btn` tracks the one mouse-held button.
- `key_held` tracks keyboard-held buttons.
- `active_knob` tracks a mouse-owned knob.
- Mouse-held button and mouse knob drag can coexist for workflows like holding `MFX` and dragging `CTRL 3`.
- Shift+drag repositions UI elements and snaps to grid.
- Renderer is still not a pure skin layer yet. It still owns:
  - physical key/mouse mapping
  - pad trigger calls into core/audio
  - UI layout persistence
  - config screen
  - project/card mount orchestration through `renderer/controller.cpp`

## Useful Commands

Search:

```bash
rg "pattern_recording" core renderer audio
```

Build:

```bash
cmake --build build
```

Run:

```bash
./build/sp303_renderer
```

Git status:

```bash
git status --short
```

## Common Regression Traps

- Display ownership: many modes compete for the 3-digit display. A late controller fallback can overwrite core display output.
- Knob ownership: `CTRL 3` means different things in different states. Do not let gain/sample-level/effect-param/MFX-type leak across modes.
- Pattern mode: `GATE`, `LOOP`, `REVERSE`, and `MARK` should not show normal sample edit state while in pattern mode.
- Cross-bank behavior: many old paths accidentally only used bank A. Always test banks B/C/D.
- MARK end-only flow: holding `MARK` while pressing a pad must suppress the normal start+end path for that playback.
- MFX: only subtypes `1..6` currently have DSP. Other subtype numbers should select/display safely but not pretend to be delay/pitch/etc.
- Core split: if you add new internal helpers for the state machine, put declarations in `core/sp303_internal.h` instead of re-hiding them inside one `.cpp`.
- Velocity: do not store performance velocity on the pad itself. It belongs to voices and pattern events.
