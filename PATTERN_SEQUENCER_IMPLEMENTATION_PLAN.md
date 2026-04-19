# Pattern Sequencer Implementation Plan

This is the saved implementation plan for the SP-303 pattern sequencer work.

The sequencer is intentionally deferred until after save/load, but this plan captures the correct order and boundaries so it can be resumed without re-thinking the whole subsystem.

## Scope

Pattern sequencing is a separate operating mode, not a small extension of sampling.

It needs:
- pattern banks A-D
- 8 pattern pads per bank
- one global pattern tempo
- pattern playback / stop / switching
- pattern recording with count-in, loop record, and overdub
- pattern setup for tempo, length, quantize, metronome level
- event erase while recording
- pattern delete / delete all / swap

## Architectural split

Keep the same overall split as the rest of the project:

- `core`
  - owns rules, mode switching, LEDs, display, bank/pad semantics
- `audio`
  - remains a playback engine for sample triggers
- `renderer/controller`
  - advances transport and bridges core sequencer events to audio sample playback

Do **not** bury full sequencer logic directly into `sp303.cpp` if it can be avoided.

## Recommended module split

Add:
- `core/pattern_sequencer.h`
- `core/pattern_sequencer.cpp`

This module should own:
- pattern storage
- event storage
- transport state
- quantize math
- record/erase/delete/swap operations

`sp303.cpp` should call into it for behavior, not store every detail inline.

## Data model

Recommended first-pass internal model:

- 32 pattern slots total
  - bank A-D x pad 1-8
- one global `pattern_tempo_bpm`
- per-pattern:
  - `assigned`
  - `length_measures`
  - `events`
- per-event:
  - `tick`
  - `sample_pad_slot` (`0..31`)

Possible later additions:
- note duration / gate-off tick
- per-event velocity
- pattern mute flags or effect-routing data if needed

## Timing model

Use an internal tick grid instead of storing event times as floating point.

Recommended:
- `96` ticks per quarter note

Why:
- easy support for quarter notes
- easy support for eighth notes
- easy support for eighth-note triplets
- easy support for sixteenth notes
- enough resolution for no-quantize-ish capture if needed later

## Quantize model

Support:
- `oFF`
- `4`
- `8`
- `8-3`
- `16`

Quantize should apply during pattern recording to the event tick that gets stored.

## Implementation phases

### Phase 1: Data model + playback-only transport

Build first:
- pattern storage
- transport clock
- loop playback
- stop behavior
- live pattern switching

User-visible behavior:
- `PATTERN SELECT` enters pattern mode
- display shows `Ptn`
- pattern bank buttons select pattern banks
- pressing a recorded pattern pad starts playback
- pressing the currently playing pattern pad stops playback
- `CANCEL` stops playback

### Phase 2: Global pattern tempo

Add:
- `TIME/BPM` while in pattern mode
- `CTRL 2` global tempo edit
- `TAP TEMPO` support in pattern mode

Tempo rules:
- one global pattern tempo only
- no per-pattern tempo storage
- match the SP-303 stepped tempo behavior where practical

### Phase 3: Pattern record setup UI

Add:
- `REC` enters pattern record setup
- target pattern bank/pad selection
- `START/END/LEVEL` + `CTRL 3` metronome volume
- `TIME/BPM` + `CTRL 2` pattern tempo
- `LENGTH` + `CTRL 3` pattern length
- `QUANTIZE` + `CTRL 3` quantize value

### Phase 4: Recording + overdub

Add:
- one-bar count-in
- display countdown `-4`, `-3`, `-2`, `-1`
- loop recording
- overdub over the same pattern
- sample bank switching during recording
- quantized event insertion

### Phase 5: Erase mode

Add:
- `DEL` while pattern recording/playback enters erase mode
- display `ErS`
- holding a sample pad erases that sample pad's events over the held window
- `DEL` exits erase mode back to normal recording

### Phase 6: Destructive pattern management

Add:
- delete single pattern
- delete all patterns
- swap pattern assignments between pads

## UI/state rules to preserve

When `PATTERN SELECT` is lit:
- bank buttons refer to pattern banks
- pads refer to pattern slots
- operations that normally affect samples should switch to pattern meaning where the manual says so

Pattern playback:
- continues after pad release
- `CANCEL` stops
- pressing the lit pad for the currently playing pattern stops
- pressing another pattern during playback switches immediately

## Main risks

The tricky parts are:
- keeping pattern-mode logic from colliding with sample-mode logic
- getting count-in and loop boundaries right
- implementing erase-over-time cleanly while transport is moving
- avoiding a giant monolithic `sp303.cpp`

## Save/load implications

Once sequencing exists, project save/load should be extended to include:
- pattern assignment
- pattern length
- global pattern tempo
- quantize settings if persisted per pattern/setup
- recorded event data

That is why sequencing and save/load need a clean shared data model.
