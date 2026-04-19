# SP-303 Manual Cross-Check Suite

This file is the manual-audit checklist for the emulator.

Goal:
- test the emulator against the SP-303 manual, not against memory
- keep a running list of what is confirmed, what mismatches, and what is still ambiguous
- give you a repeatable set of workflows to run after changes

Use these status tags:
- `[PASS]` manual and emulator agree
- `[FAIL]` emulator behavior is wrong
- `[PARTIAL]` partly implemented or missing an edge case
- `[AMBIGUOUS]` manual wording is unclear or hardware behavior needs external confirmation
- `[UNTESTED]` not checked yet

Recommended testing rules:
- start from a fresh launch when possible
- use at least:
  - one short one-shot sample
  - one longer loop/phrase sample
  - one stereo sample if available
- test in bank A first, then repeat selected tests in bank B/C/D
- when a workflow mentions "while sound is still playing", use a sample long enough to make that practical

## Audit Table Format

For each item below, fill in:
- `Status:`
- `Observed:`
- `Notes:`

If an item ends up unclear, copy it into the `Ambiguities` section at the bottom.

---

## 1. Boot / Idle

### 1.1 Power-on display

Manual basis:
> Pattern Bank A is selected when the power is turned on.

Expected:
- bank A selected on launch
- neutral display should be `---` in our emulator baseline
- no stale edit/record/delete/pattern state survives boot

Actual implementation:
- bank A is the default active bank
- display is initialized to `---`
- no pads should be falsely lit at rest

Test:
1. Launch the program.
2. Confirm bank A is lit.
3. Confirm display shows `---`.
4. Confirm `REC`, `DEL`, `MARK`, `PATTERN SELECT`, `RESAMPLE` are not lit.
5. Confirm no pad is stuck lit.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 1.2 Idle pad press feedback

Manual basis:
- no direct quote captured for "pad stays lit for N seconds" because that is emulator UI feedback, not hardware text

Expected:
- pressing any pad in idle gives visible feedback
- filled pad plays
- empty pad still lights for visual confirmation
- pad LED hold should be `sample playback duration + 3 seconds`

Actual implementation:
- current emulator intends exactly that LED hold rule

Test:
1. Press an empty pad in IDLE.
2. Confirm it lights.
3. Press a filled short sample.
4. Confirm the pad stays lit for sample duration plus about 3 seconds.
5. Repeat with a longer sample.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 2. Sampling

### 2.1 Basic sampling workflow

Manual quote:
> Press the [REC], and confirm that the button has lit.
> The pads to which you can sample then blink, and the SP-303 goes into sampling standby.

Manual quote:
> Press [REC].
> [REC] lights up, and sampling starts.

Manual quote:
> When you reach the point where you want sampling to stop, press [REC].

Expected:
1. `REC` from IDLE enters standby
2. available target pads blink
3. selecting a target pad makes it solid
4. pressing `REC` again arms or starts the sample workflow according to current standby/threshold logic
5. pressing `REC` again ends sampling
6. recorded pad gets the sample

Actual implementation:
- standby, ready, recording, and assignment exist
- threshold-aware path exists: standby shows `rdY` before threshold trigger
- finished sample should be assigned to selected pad

Test:
1. Press `REC` from IDLE.
2. Confirm empty pads blink.
3. Select an empty pad.
4. Confirm selected pad is solid.
5. Press `REC` again.
6. Confirm display shows `rdY` if threshold logic is active.
7. Feed input above threshold.
8. Confirm recording starts.
9. Press `REC` to stop.
10. Press the target pad to confirm playback.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 2.2 Sampling quality and stereo

Manual quote:
> [LONG/LO-FI] not lit: STANDARD
> [LONG/LO-FI] lit: LONG
> [LONG/LO-FI] blinking: LO-FI

Manual quote:
> [STEREO] lit: Stereo sampling
> [STEREO] not lit: Mono sampling

Expected:
- default sampling mode is mono
- `STEREO` lit means stereo capture
- `LONG/LO-FI` cycles through standard, long, lofi
- these controls are relevant during sampling standby, not idle display clutter

Actual implementation:
- mono default is intended
- quality mode is stored and used by the audio engine
- user previously requested that `LONG/LOFI` should never stay lit in pure IDLE

Test:
1. Enter sample standby.
2. Toggle `STEREO` and confirm the light reflects the state.
3. Toggle `LONG/LOFI` through three states and confirm not lit / lit / blinking.
4. Exit back to IDLE.
5. Confirm `LONG/LOFI` is not lit in IDLE.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 2.3 Pre-set BPM sampling

Manual quote:
> Press [TIME/BPM], and confirm that the button has lit.

Manual quote:
> Turn the CTRL 2 (BPM) knob to set the BPM.
> Settings range: 40–200

Manual quote:
> When sampling is finished, the End Point is automatically set according to the BPM, and [MARK] lights up

Expected:
- in sampling standby, `TIME/BPM` toggles BPM edit
- CTRL 2 sets 40 to 200 BPM
- tap tempo can also set BPM
- after recording finishes, sample end is quantized to the chosen BPM and `MARK` lights

Actual implementation:
- BPM arming before record exists
- post-record end quantization hook exists
- this needs manual validation after real sampling

Test:
1. Enter sampling standby.
2. Press `TIME/BPM`.
3. Turn CTRL 2 and confirm numeric BPM display.
4. Exit `TIME/BPM`.
5. Record a sample.
6. Confirm end point snaps to BPM and `MARK` lights.
7. Compare resulting pad BPM display against expectation.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 3. Threshold Sampling Mode

### 3.1 Enter / edit / exit threshold mode

Manual basis:
- emulator feature, not directly quoted from the manual

Expected:
- from IDLE only: hold `CANCEL` and press `REC`
- display shows `-n-`, where `n` is `0..8`
- `REC` flashes while in threshold mode
- CTRL 3 changes threshold
- pressing `REC` exits threshold mode to `---`

Actual implementation:
- threshold mode exists globally
- sample level threshold is persistent

Test:
1. From IDLE, hold `CANCEL` and press `REC`.
2. Confirm threshold mode enters.
3. Turn CTRL 3 across the full range.
4. Confirm display values `0..8`.
5. Press `REC`.
6. Confirm return to `---`.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 3.2 Threshold-triggered start / silence stop

Manual basis:
- emulator design agreed during development

Expected:
- in standby/ready, recording begins only when input crosses threshold
- normal sampling auto-stops after sustained silence
- resampling must not use silence auto-stop

Actual implementation:
- normal sample path uses threshold start and silence stop
- resample path is supposed to stop only on `REC`

Test:
1. Set threshold high.
2. Enter sample standby and ready.
3. Feed low-level noise and confirm no start.
4. Feed loud transient and confirm start.
5. Let tail decay and confirm auto-stop.
6. Repeat in resample mode and confirm no silence auto-stop.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 4. MARK / Start / End

### 4.1 Adjust both start and end

Manual quote:
> At the location where you wish to set the Start Point, press [MARK].
> [MARK] blinks, and “---” appears in the display.

Manual quote:
> At the location where you wish to set the End Point, press [MARK] once again.
> When [MARK] lights, the setting is complete.

Expected:
- press pad so sample is playing
- press `MARK` at desired start point
- display becomes `---`, `MARK` blinks
- press `MARK` again at desired end point
- final sample region is stored

Actual implementation:
- two-stage mark edit exists
- there were earlier bugs around very short samples and wrong end-only behavior

Test:
1. Play a sample long enough to mark.
2. Press `MARK` mid-playback for start.
3. Press `MARK` again later for end.
4. Retrigger sample and confirm trimmed region only.
5. Confirm `MARK` light remains on after trimmed region is set.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 4.2 Adjust only end

Manual quote:
> Hold down [MARK] and press the pad to which the sample you wish to change is assigned.

Manual quote:
> Release your finger from [MARK].
> [MARK] blinks, and “---” appears in the display.

Manual quote:
> At the location where you wish to set the End Point, press [MARK].

Expected:
- holding `MARK` while pressing the pad should arm end-only mode
- the second `MARK` sets end, not start

Actual implementation:
- this was previously wrong and was explicitly fixed

Test:
1. Hold `MARK`.
2. Press a playing pad.
3. Release `MARK`.
4. Press `MARK` at desired end point.
5. Confirm beginning of sample is unchanged and only the end moved.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 4.3 Adjust only start

Manual quote:
> At the location where you wish to set the Start Point, press [MARK].
> [MARK] will blink.

Manual quote:
> Press the pad again
> When [MARK] lights, the setting is complete.

Expected:
- first `MARK` stores new start
- pressing the pad again finalizes using the sample end as the end point

Actual implementation:
- start-only and dual-point logic are partly merged in the emulator
- this needs targeted validation

Test:
1. Play a sample.
2. Press `MARK` at desired start.
3. Press pad again to end playback / finalize.
4. Retrigger and confirm sample starts later but still plays to the natural end.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 4.4 MARK reset behavior

Manual quote:
> If the setting was not made as desired, press the lit [MARK] button while the sound is still playing to make it go dark, and re-do the procedure from step 1.

Expected:
- reset only works if:
  - `MARK` is lit
  - the sample is still actively playing
- pressing `MARK` while no audio is playing should do nothing

Actual implementation:
- this exact gating was recently fixed

Test:
1. Trim a sample so `MARK` is lit.
2. Retrigger sample.
3. While it is still playing, press `MARK`.
4. Confirm start/end reset to full range.
5. Wait until playback stops.
6. Press `MARK` again.
7. Confirm nothing happens.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 4.5 Short-sample edge case

Manual basis:
- development bug report, not a direct manual quote

Expected:
- pressing `MARK` very close to sample end should not freeze or crash
- start and end should never collapse to an invalid tiny region

Actual implementation:
- freeze was reportedly fixed, but this is exactly the kind of case to keep retesting

Test:
1. Use a very short sample.
2. Press `MARK` near the last 100 ms of playback.
3. Repeat several times with both normal and end-only workflows.
4. Confirm no freeze, no crash, no stuck UI.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 5. START / END / LEVEL Fine Edit

### 5.1 Enter / adjust / exit

Manual quote:
> Press [START/END/LEVEL], and confirm that the button has lit.

Manual quote:
> Turn the CTRL 1 (START) knob.
> Turn the CTRL 2 (END) knob.
> Turn the CTRL 3/MFX (LEVEL) knob to adjust the sample’s volume.

Manual quote:
> Press [START/END/LEVEL], and confirm that the button has turned off.

Expected:
- last played pad becomes current edit target
- display shows `Edt` on entry
- CTRL 1 = start, CTRL 2 = end, CTRL 3 = level
- numeric display persists after movement until exit
- pressing `START/END/LEVEL` again exits
- pressing any other pad/button exits edit mode, except retriggering the same last pad

Actual implementation:
- level and exit logic were explicitly fixed earlier
- persistent numeric display was also requested and implemented

Test:
1. Play a sample.
2. Press `START/END/LEVEL`.
3. Turn CTRL 1, CTRL 2, CTRL 3 separately.
4. Confirm numeric display stays visible after you stop moving the knob.
5. Retrigger the same pad and confirm edit mode stays active.
6. Press a different pad and confirm edit mode exits to `---`.
7. Press `START/END/LEVEL` again and confirm exit to `---`.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 6. TIME / BPM Sample Edit

### 6.1 Sample BPM correction

Manual quote:
> If you want to halve the displayed value, turn the knob to the left; turn the knob to the right if you want to double the value.

Expected:
- in sample `TIME/BPM` mode:
  - CTRL 2 below 25% = base / 2
  - CTRL 2 above 75% = base * 2
  - middle band = base

Actual implementation:
- this exact three-zone behavior was requested and restored

Test:
1. Play a pad and note the displayed BPM.
2. Press `TIME/BPM`.
3. Move CTRL 2 below 25%.
4. Confirm display shows half.
5. Move CTRL 2 to middle.
6. Confirm display shows base BPM.
7. Move CTRL 2 above 75%.
8. Confirm display shows double BPM.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 6.2 Time modify

Manual quote:
> Turn the CTRL 1 (TIME) knob.

Manual quote:
> When the knob is turned completely to the left, Time Modify is turned off. (“oFF” appears in the display.)

Manual quote:
> When the knob is turned completely to the right, samples are played at the tempo set in the pattern. (“Ptn” appears in the display.)

Expected:
- CTRL 1 in `TIME/BPM` mode controls time modify
- far left = `oFF`
- far right = `Ptn`
- mid positions show a BPM-derived number
- display should persist until knob meaning changes again or mode exits

Actual implementation:
- WSOLA-style time mode exists
- `oFF` and `Ptn` display modes exist

Test:
1. Press `TIME/BPM` on a sample.
2. Turn CTRL 1 fully left and confirm `oFF`.
3. Turn CTRL 1 to mid and confirm a numeric BPM-like value.
4. Turn CTRL 1 fully right and confirm `Ptn`.
5. Stop moving the knob and confirm display remains on the current time-mode value.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 6.3 Exit behavior

Manual quote:
> Press [TIME/BPM] again.

Expected:
- pressing `TIME/BPM` exits the mode
- playing a different pad exits the mode

Actual implementation:
- explicit exit-on-other-pad was requested

Test:
1. Enter `TIME/BPM`.
2. Press a different sample pad.
3. Confirm the mode exits.
4. Re-enter `TIME/BPM`.
5. Press `TIME/BPM` again.
6. Confirm exit.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 7. Sample Playback Modes

### 7.1 Loop vs one-shot

Manual quote:
> [LOOP] lit: Loop Playback
> [LOOP] not lit: One Shot Playback

Expected:
- `LOOP` toggles loop behavior on current sample

Actual implementation:
- per-pad loop mode exists

Test:
1. Play a longer sample.
2. Toggle `LOOP` on.
3. Retrigger and confirm it repeats.
4. Toggle `LOOP` off.
5. Retrigger and confirm it plays once.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 7.2 Gate vs trigger

Manual quote:
> [GATE] lit: Gate Playback
> [GATE] not lit: Trigger Playback

Expected:
- gate playback stops when pad/key is released
- trigger playback continues after release

Actual implementation:
- gate mode is wired through to audio note-off logic

Test:
1. Set `GATE` on for a sample.
2. Hold the pad and release early.
3. Confirm playback stops on release.
4. Set `GATE` off.
5. Press and release quickly.
6. Confirm playback continues.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 7.3 Reverse

Manual quote:
> [REVERSE] lit: Reverse Playback

Expected:
- toggling `REVERSE` flips playback direction for current sample
- start and end semantics reverse accordingly

Actual implementation:
- reverse mode exists, but this has regressed before and needs repeated checking

Test:
1. Play a recognizable sample forward.
2. Toggle `REVERSE`.
3. Retrigger and confirm the sample is audibly reversed.
4. Toggle `REVERSE` off and confirm normal playback returns.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 8. Effects

### 8.1 Effect on / off / switch

Manual basis:
- effect workflow has been approximated in the emulator rather than fully hardware-faithful

Expected:
- selecting an effect changes the sound on effect-routed pads
- pressing the same effect again should turn it off cleanly
- switching from one effect to another should take one action, not two

Actual implementation:
- effect off/on leakage was fixed recently
- there was also a one-extra-click bug when switching effects

Test:
1. Play sample dry.
2. Enable `FILTER+DRIVE`.
3. Retrigger and confirm audible change.
4. Disable `FILTER+DRIVE`.
5. Retrigger and confirm dry playback.
6. Switch directly from `PITCH` to `FILTER+DRIVE`.
7. Confirm only one click is needed and the newly selected effect is active.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 8.2 Effect labels

Manual / design basis:
- emulator display convention added during development

Expected:
- `PITCH` knob labels:
  - CTRL 1 = `Pit`
  - CTRL 2 = `Fdb`
  - CTRL 3 = `dAL`
- `FILTER+DRIVE` knob labels:
  - CTRL 1 = `CoT`
  - CTRL 2 = `Fdb` or resonance/feedback label in the emulator convention
  - CTRL 3 = `dRV`
- effect labels must not overwrite `TIME/BPM` or sample-edit displays

Actual implementation:
- those labels were explicitly added

Test:
1. Enable `PITCH`.
2. Move each effect knob and confirm the right label.
3. Enter `TIME/BPM` and confirm effect labels stop taking over the display.
4. Enter `START/END/LEVEL` and confirm effect labels stop taking over there too.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 9. Resampling

### 9.1 Core resample workflow

Manual basis:
- emulator now follows the user-defined workflow, which is close to the manual but intentionally adapted

Expected:
1. press `RESAMPLE`
2. display shows `LEV`
3. pads can still be played here
4. press `REC`
5. empty destination pads blink
6. select destination pad
7. press `REC` again
8. filled source pads blink
9. play source pads to record the resample
10. press `REC` to stop

Actual implementation:
- this flow exists in the current state machine

Test:
1. Put drums on several pads.
2. Press `RESAMPLE`.
3. Confirm display `LEV`.
4. Play a few pads and confirm they still sound and light.
5. Press `REC`.
6. Pick an empty pad.
7. Press `REC` again.
8. Play a drum loop using multiple source pads.
9. Press `REC` to stop.
10. Confirm destination pad now contains the new sample.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 9.2 Resample gain, stereo, quality, stop behavior

Manual quote:
> Turn the CTRL 3/MFX (LEVEL) knob to adjust the sampling level

Expected:
- in resample source/dest/armed states, CTRL 3 shows `0..127` gain values
- `STEREO` and `LONG/LOFI` can be set before recording begins
- once resample recording is actually active, resampling stops only when `REC` is pressed

Actual implementation:
- this behavior was specifically requested and should now exist

Test:
1. Enter resample source mode.
2. Move CTRL 3 and confirm display changes to numeric `0..127` and does not snap back immediately to `LEV`.
3. Before starting recording, toggle `STEREO` and `LONG/LOFI`.
4. Start resampling.
5. Feed silence.
6. Confirm it does not auto-stop.
7. Press `REC` to stop.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 10. Delete / Truncate / Swap Samples

### 10.1 Delete single sample

Manual quote:
> Press [DEL], and confirm that the button has lit.

Manual quote:
> Press the pad ... [DEL] blinks.

Manual quote:
> Press [DEL].

Expected:
- from IDLE:
  - `DEL` enters delete mode and shows `dEL`
  - filled pads blink
  - choosing a pad arms deletion
  - pressing `DEL` again deletes it

Actual implementation:
- exists

Test:
1. Press `DEL`.
2. Confirm `dEL`.
3. Confirm filled pads blink.
4. Press one filled pad.
5. Confirm it goes solid and `DEL` blinks.
6. Press `DEL` again.
7. Confirm sample is removed.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 10.2 Truncate

Manual quote:
> Make sure that the Start and End Points have been set ([MARK] is lit).

Manual quote:
> Press [DEL], and confirm that the button has lit.

Manual quote:
> Press [MARK].
> [DEL] blinks, and “trC” appears in the display.

Manual quote:
> Press [DEL].

Expected:
- truncated sample keeps only the marked region

Actual implementation:
- truncate command exists
- loading/progress dots intentionally not implemented and already noted in TODO

Test:
1. Set start/end so `MARK` is lit.
2. Press `DEL`.
3. Press `MARK`.
4. Confirm `trC`.
5. Press `DEL`.
6. Confirm sample duration shrinks to the marked region.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 10.3 Delete all samples

Manual quote:
> Hold down [CANCEL] and press [DEL].
> “dAL” appears in the display.

Expected:
- `CANCEL + DEL` enters delete-all
- choosing A/B means internal set 0..15
- choosing C/D means external/card set 16..31 in emulator terms

Actual implementation:
- exists for samples

Test:
1. Fill at least one sample in A/B and one in C/D.
2. Hold `CANCEL`, press `DEL`.
3. Select A or B.
4. Press `DEL` to confirm.
5. Confirm only A/B side is cleared.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 10.4 Swap samples

Manual quote:
> While holding down [DEL], press [REC].
> “CHG” appears in the display.

Expected:
- choose first sample pad
- choose second sample pad
- press `REC` to confirm swap

Actual implementation:
- exists for samples

Test:
1. Put two clearly different samples on two pads.
2. Hold `DEL` and press `REC`.
3. Select first pad.
4. Select second pad.
5. Press `REC`.
6. Confirm assignments are swapped.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 11. Pattern Sequencer

### 11.1 Enter / exit pattern mode

Manual quote:
> Press [PATTERN SELECT], and confirm that the button has lit.
> “Ptn” appears in the display.

Expected:
- `PATTERN SELECT` enters pattern mode
- display shows `Ptn`
- pressing it again exits pattern UI

Actual implementation:
- `PATTERN SELECT` UI state exists
- previous light-off bug was fixed

Test:
1. Press `PATTERN SELECT`.
2. Confirm button lights and display shows `Ptn`.
3. Press `PATTERN SELECT` again.
4. Confirm light goes off and display returns to `---`.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.2 Pattern playback / stop / switch

Manual quote:
> Once playback begins, the pattern continues playing even after you release the pad.

Manual quote:
> If you want to stop a pattern during playback, press [CANCEL].

Manual quote:
> The pattern currently being played back stops, and playback of the new pattern immediately begins.

Expected:
- hitting a recorded pattern starts playback
- it continues after release
- `CANCEL` stops it
- hitting another recorded pattern switches immediately
- hitting the currently playing pattern stops it

Actual implementation:
- transport exists
- bank-addressing bug was just fixed in pattern path

Test:
1. Record at least two patterns.
2. Start one.
3. Release pad and confirm it keeps going.
4. Press `CANCEL` and confirm stop.
5. Start first pattern again.
6. Press second pattern and confirm immediate switch.
7. Press the currently playing pattern and confirm stop.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.3 Pattern record setup

Manual quote:
> Press [REC], and confirm that the button has lit.

Manual quote:
> Press [START/END/LEVEL] ... Turn the CTRL 3/MFX (LEVEL) knob to adjust the metronome volume.

Manual quote:
> Press [TIME/BPM] ... Turn the CTRL 2 (BPM) knob to adjust the pattern's tempo.

Manual quote:
> Press [LENGTH] ... Turn the CTRL 3/MFX knob to adjust the pattern length.

Manual quote:
> Press [QUANTIZE] ... Turn the CTRL 3/MFX knob to set the quantization.

Expected:
- from pattern mode:
  - `REC` enters pattern record setup
  - empty pattern pads blink
  - selected target pattern pad goes solid
  - `START/END/LEVEL` edits metronome level
  - `TIME/BPM` edits global pattern BPM
  - `LENGTH` edits measures
  - `QUANTIZE` edits quantize mode

Actual implementation:
- setup path exists

Test:
1. Enter pattern mode.
2. Press `REC`.
3. Select an empty pattern pad.
4. Toggle `START/END/LEVEL` and edit metronome level.
5. Toggle `TIME/BPM` and edit BPM.
6. Toggle `LENGTH` and edit bars.
7. Toggle `QUANTIZE` and cycle `oFF`, `4`, `8`, `8-3`, `16`.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.4 Pattern count-in and recording

Manual quote:
> One measure of a count sound is inserted before recording actually begins

Manual quote:
> a countdown showing “-4, -3, -2, -1” appears in the display

Expected:
- pressing `REC` again from record setup starts count-in
- one-bar count-in sounds
- display shows `-4 -3 -2 -1`
- actual recording starts after count-in

Actual implementation:
- count-in exists
- metronome click and accent were recently adjusted

Test:
1. Enter pattern record setup.
2. Press `REC` to begin.
3. Confirm count-in display.
4. Confirm metronome clicks, with beat 1 accented.
5. Press sample pads after count-in and confirm events are recorded.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.5 Loop record / overdub / quantize

Manual quote:
> If the number of measures indicated in the display exceeds the value set for the pattern length, recording continues automatically after returning to the first measure (Loop Recording).

Manual quote:
> The previously recorded pad performances and the performances for the pads currently pressed are layered and recorded together (overdubbing).

Expected:
- pattern loops after configured length
- previously recorded notes play while new notes are added
- quantize setting affects recorded timing

Actual implementation:
- basic loop and overdub model exists

Test:
1. Record a one-bar pattern with a kick on beat 1.
2. Let it loop.
3. Overdub snare on beat 3.
4. Stop recording.
5. Play pattern back and confirm both events exist.
6. Repeat with different quantize modes.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.6 Pattern erase while recording

Manual quote:
> Press [DEL], and confirm that the button has lit.
> “ErS” appears in the display.

Manual quote:
> Press the pad with the sample you want to erase at the time it is to be erased.

Expected:
- while pattern recording is running:
  - `DEL` enters erase mode and shows `ErS`
  - holding a sample pad erases that pad's events over time
  - pad hold in erase mode should erase, not retrigger sample playback
  - `DEL` exits erase mode

Actual implementation:
- this was just added

Test:
1. Record a simple repeating pattern with kick on pad 1.
2. Start pattern recording again for overdub.
3. Press `DEL`.
4. Confirm `ErS`.
5. Hold pad 1 across a region where kick events occur.
6. Exit erase mode.
7. Stop recording and play back.
8. Confirm those pad-1 events were removed.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

### 11.7 Pattern delete / delete all / swap

Manual quote:
> Press [DEL], and confirm that the button has lit.
> “dEL” appears in the display.

Manual quote:
> While holding down [CANCEL], press [DEL].
> “dAL” (Delete ALL) appears in the display.

Manual quote:
> While holding down [DEL], press [REC].
> [DEL] and [REC] light up, and “CHG” appears in the display.

Expected:
- single delete clears one pattern
- delete-all clears one pattern memory group
- swap exchanges two pattern slots

Actual implementation:
- these were just added

Test:
1. Record two different patterns.
2. Test single delete on one.
3. Re-record it.
4. Test swap between the two.
5. Test delete-all for A/B group or C/D group.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 12. Save / Load

### 12.1 Project persistence

Manual basis:
- emulator project save/load, not SP-303 SmartMedia parity yet

Expected:
- quicksave or project load should restore:
  - pad sample presence
  - playback flags
  - edit data
  - effect routing
- app must remain responsive after load

Actual implementation:
- previous freeze bugs happened here, so this stays in the suite permanently

Test:
1. Create a project with multiple pads populated.
2. Save it.
3. Load it.
4. Confirm app remains responsive to mouse and keyboard.
5. Retrigger several pads.
6. Confirm samples and settings survived.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## 13. Weird Combo / State-Leak Checks

These are not direct manual steps. They exist because this project is state-machine heavy and regressions tend to come from mode leakage.

Expected:
- knob ownership should be state-specific
- pressing unrelated buttons should not leak stale state into the next mode
- display ownership should be clear and mode-correct

Test:
1. Enter and exit `TIME/BPM`, then immediately enter `START/END/LEVEL`.
2. Enter and exit `RESAMPLE`, then enter normal sample standby.
3. Enter pattern mode, then leave it, then trigger samples.
4. Hold unusual button combos: `DEL+REC`, `CANCEL+DEL`, `MARK+PAD`, `TIME/BPM+TAP`.
5. Confirm no stuck LEDs, no stuck display, no frozen input, and no leaked knob meaning.

Status:
- `[UNTESTED]`

Observed:
- 

Notes:
- 

---

## Ambiguities To Confirm Elsewhere

Copy unresolved items here once you hit one.

Template:
- Item:
- Why ambiguous:
- Suspected options:
- Ask on:
  - `sp-forums.com`
  - YouTube hardware demo
  - service notes / other manual pages

Current seed ambiguities:
- Pattern delete-all mapping to emulator bank groups versus real internal/card memory semantics.
- Exact hardware LED behavior for some pattern-management states.
- Whether pattern delete should preserve per-pattern setup defaults or fully reset the slot exactly as power-on state.
- Some effect workflows are intentionally approximate in the emulator and may differ from hardware.
