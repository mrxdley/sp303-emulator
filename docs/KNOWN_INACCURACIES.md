# Known Inaccuracies

This file is for behavior that is not yet faithful to the SP-303, or is intentionally emulator-specific.

## Missing

- Remaining `MFX 7..21` have no DSP yet.
- MIDI input is not implemented.
- Manual-faithful review of all remaining edge cases is still incomplete.
- `MARK` gate-playback behavior still needs verification and cleanup.
- `trC` pad-lighting feedback still needs review.
- Effect-grab functionality is still missing.

## Conveniences

- Project quicksave/load exists as an emulator convenience.
- Virtual memory card folder selection is an emulator convenience, even though it models SmartMedia-style storage.
- Pattern tap-tempo BPM display timeout is debug-friendly rather than hardware-faithful.
- `PITCH` is intentionally playback-speed based, not a hardware-style pitch shifter.
- Some Android/mobile UI affordances are purely pragmatic and not hardware-like.

## Other

- `FILTER+DRIVE`:
  - drive at `0` should fully bypass, but still passes through processing
  - fast movement at low cutoffs can crackle because coefficients are not smoothed enough
- `HOLD`:
  - retriggering a latched gate sample does not currently clear/re-arm the hold state automatically
  - current behavior is usable, but likely not the final intended behavior
- BPM detection is heuristic and still not guaranteed to match the original machine’s feel.
- `PITCH` playback-down behavior still needs review for possible early-stop/truncation feel.
- `VINYL SIM` is not yet considered accurate.
- It is still unclear whether effect assignment to empty pads should be allowed in every case; this needs manual verification.
- Knob ownership/state isolation has improved a lot, but mode leakage still needs continuous regression testing.
