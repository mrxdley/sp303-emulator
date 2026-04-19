# Pattern Sequencer Research Notes

This note records the manual-derived behavior we need before implementing the pattern sequencer.

## What Pattern Mode Means

- `PATTERN SELECT` changes the meaning of BANK A-D and Pads 1-8.
- Each bank holds 8 patterns.
- Four banks exist total: A-D.
- Pattern Bank A is selected at power-on.

## Playback Behavior

- Pressing a recorded pattern pad starts playback.
- Playback continues after pad release.
- Pressing `CANCEL` stops the pattern.
- Pressing the lit pad for the current pattern stops it.
- Switching to another recorded pattern should start the new one immediately and exit pattern-select state.

## Tempo Behavior

- Pattern tempo is global, not per pattern.
- `TIME/BPM` in pattern mode edits the pattern tempo.
- `CTRL 2` changes BPM in the `40-200` range.
- `TAP TEMPO` should work in pattern mode.

## Record Setup

- `REC` in pattern mode should enter pattern record setup.
- The manual expects:
  - bank selection
  - target pad selection
  - metronome level
  - tempo
  - length
  - quantize

## Record Behavior

- Pattern recording uses count-in.
- The display shows `-4`, `-3`, `-2`, `-1` during count-in.
- Quantized pad hits are stored as events.
- Loop recording / overdub is expected.
- Recording stops on `REC`.

## Edit / Delete / Swap

- Erase performance data while recording.
- Delete single pattern.
- Delete all patterns.
- Swap pattern assignments between pads.

## Minimal Implementation Order

1. Data model for patterns and events.
2. Playback transport.
3. Pattern tempo UI.
4. Record setup.
5. Recording and overdub.
6. Erase.
7. Delete and swap.

## Open Design Questions

- Should pattern events store only pad triggers, or also hold/gate length?
- Should pattern tempo live in the global device state or in the active pattern bank?
- Should the transport live in core or the renderer/controller layer?
- Should pattern save/load reuse the project format already used for samples?

