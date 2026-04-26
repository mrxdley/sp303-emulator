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
- The pitch path still needs a hard review for early-stop behavior when samples are pitched down.
- Current suspected issue:
  - the pitch effect stretches playback in time by changing playback rate
  - but the voice still stops against the sample's normal trimmed end condition
  - this can make pitched-down playback feel truncated even though there is no separate "hard time cap"
  - fix later by auditing the pitch playback cursor and stop condition together, not independently

### MFX Coverage Note
- Hardware MFX provides 21 distinct algorithms.
- Current emulator implements:
  - `MFX 1 = reverb`
  - `MFX 2 = tape echo`
  - `MFX 3 = chorus`
  - `MFX 4 = flanger`
  - `MFX 5 = phaser`
  - `MFX 6 = tremolo/pan`
- `mfx_type` still ranges `1..21` in the UI/chord, but only subtype 1 currently resolves to DSP.
- `mfx_type` still ranges `1..21` in the UI/chord, but subtypes `7..21` still do not resolve to DSP yet.
- Add the remaining real MFX algorithms later:
  - wah
  - slicer
  - compressor-style variants
  - and the rest of the SP-303 MFX family
- Do not alias dedicated effects like `PITCH` or `DELAY` through MFX.

### Knob Ownership
- Knob meaning should be explicit per state.
- Do not let CTRL 3 / MFX meaning leak across unrelated modes.
- Recording gain, sample level, effect params, and pattern edit controls should be state-local, not globally shared if the mode already defines them.

### Pattern / Tap Tempo Notes
- Pattern tap-tempo BPM is now displayed for 3 seconds after enough taps are collected.
- Treat that value as debug-helpful, not hardware-accurate, so add it to conveniuences.
- The tap BPM is derived from a short 4-tap average and may drift from the original unit's feel or rounding behavior.

### Convenience Features
- Consider a non-manual convenience mode for `START/END/LEVEL` sample `LEVEL`.
- Manual-faithful behavior is `0..127` with `127 = original recorded level` and no boost above unity.
- Convenience option to explore later:
  - treat a middle value as neutral and allow gain boost above the original sample level
  - keep this clearly marked as emulator-only behavior, not SP-303-authentic behavior
- Consider emulator-only resampling of live pattern playback.
- Keep this explicitly marked as a convenience feature unless the manual confirms an equivalent workflow.
## Truncate Workflow (From SP-303 Manual)
- Implement sample truncate mode that deletes unused waveform outside current Start/End points.
- Trigger flow:
  - `IDLE`: press pad with sample (becomes current pad).
  - Ensure Start/End are set (`MARK` lit).
  - Press `DEL` (lit).
  - Press `MARK` -> `DEL` blinks, display `trC`.
  - Press `DEL` again to confirm truncate.
- On confirm: rewrite sample data to keep only current playback region, then reset region to full sample.
- Keep this as separate behavior 
### Scope Note

- Do **not** implement dots/loading progress animation or artificial delays.
- Truncate can complete instantly in this emulator.

## Behavior Research

- Check SP-Forums/manual behavior for `LONG/LO-FI` pressed during recording and across other states, then align state-machine behavior.
- MARK gate-playback logic still needs manual verification and implementation review.
- Known UI issue: when a pad is pressed while `MARK` is held, pad-lit feedback is not currently correct.
- Truncate (`trC`) workflow still needs a pad-lighting review so the visual feedback matches the intended operation.

stub velocity for MIDI input later
there is innacuracies in how vinyl sim is implemented -> gain on CMP
ADD effect grab functionality.
check all instances of tapp tempo
figure out: can we apply FX to a pad with no sample? i think so?
