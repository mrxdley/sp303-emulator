# TODO

## Implemented Features (From Previous Work)

### Effect System Architecture
- [x] 5 direct-button effects: FILTER+DRIVE, PITCH, DELAY, VINYL_SIM, ISOLATOR
- [x] 21 MFX types (1-21) with registry dispatch
- [x] Per-voice effects: each pad gets its own effect instance
- [x] Only one effect type active at a time (true to SP-303)

### FILTER+DRIVE Effect
- CTRL1: CUTOFF (50Hz-20kHz exponential)
- CTRL2: RESONANCE (Q 0.5-20)
- CTRL3: DRIVE (0=bypass, 1=heavy distortion)
- **Issue**: DRIVE at 0 should completely bypass, currently still processes through tanh
- **Issue**: Filter crackles at low cutoffs with fast knob movement (needs coefficient smoothing)

### REMAIN Button for FX Copying
- Hold REMAIN + press Pad = toggle effect on/off for that pad
- Hold REMAIN + press effect button = toggle "all-pads" mode
- When REMAIN held, all pads with the effect light up to show routing

### Effect Button Logic (Per-Pad)
- Effect button lights only if effect is on the CURRENTLY PLAYING pad
- Press effect button on new pad → effect applies to that pad
- Press effect button on pad that already has effect → effect turns off
- No "ghost press" issues - state checks current pad

### Resampling Workflow
- [ ] RESAMPLE button enters resampling mode
- [ ] Select source pad (playing with effect)
- [ ] Select destination pad (empty or overwrite)
- [ ] Press REC to arm, play source to record post-FX audio
- [ ] Press REC again to stop, assigns to destination pad

### Display
- [x] Shows parameter names (CoF, rES, drV) not values when adjusting effect knobs
- [x] 7-seg codes for all effect parameter mnemonics

## Code Cleanup Tasks

### Remove sp303_ Prefix
- Replace all `sp303_` prefixed functions with `sp303::` namespace
- Rename types: SP303Device→Device, SP303State→State, etc.
- Enums: SP303_EFFECT_FILTER_DRIVE → sp303::EFFECT_FILTER_DRIVE

### Effect File Structure
Create individual files for each effect:
```
audio/effects/
  effects.h        - FxState, FxParams, Effect interface
  effects.cpp      - Registry and dispatch
  filter_drive.cpp - FILTER+DRIVE implementation
  pitch.cpp        - PITCH stub
  delay.cpp        - DELAY stub  
  vinyl_sim.cpp    - VINYL_SIM stub
  isolator.cpp     - ISOLATOR stub
  reverb.cpp       - REVERB stub
  tape_echo.cpp    - TAPE_ECHO stub
  chorus.cpp       - CHORUS stub
  mfx_stubs.cpp    - Remaining MFX stubs
```

### Pitch Effect Note
- `PITCH` is intentionally implemented as playback-speed change, not a hardware-faithful pitch-shift algorithm.
- This means pitch and duration change together.
- Keep this note in mind if we later replace it with a better shifter.

## Truncate Workflow (From SP-303 Manual)

- Implement sample truncate mode that deletes unused waveform outside current Start/End points.
- Trigger flow:
  - `IDLE`: press pad with sample (becomes current pad).
  - Ensure Start/End are set (`MARK` lit).
  - Press `DEL` (lit).
  - Press `MARK` -> `DEL` blinks, display `trC`.
  - Press `DEL` again to confirm truncate.
- On confirm: rewrite sample data to keep only current playback region, then reset region to full sample.
- Keep this as separate behavior from pad-delete mode (`DEL` + pad delete flow).

### Scope Note

- Do **not** implement dots/loading progress animation or artificial delays.
- Truncate can complete instantly in this emulator.

## Behavior Research

- Check SP-Forums/manual behavior for `LONG/LO-FI` pressed during recording and across other states, then align state-machine behavior.

stub velocity for MIDI input later
resampling LVL - 0-127 persisitent
