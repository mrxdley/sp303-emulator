# Velocity Manual Test Suite

This file covers the current engine-side velocity implementation and the later MIDI-facing behavior it was designed for.

## Ground Rules

- Velocity is stored per trigger / per voice.
- Velocity is stored in pattern event data.
- Velocity is attenuation-only.
- Velocity must never make a sample louder than the level already set on that pad.
- Current keyboard/mouse UI still records and plays at fixed velocity `127`.

## 1. Regression: Existing Non-MIDI Pad Playback

Purpose: prove current workflows sound unchanged.

Steps:
1. Load or record a sample onto pad A1.
2. Set `START/END/LEVEL` sample level to an obvious value, e.g. `80`.
3. Play the pad repeatedly from keyboard and mouse.
4. Record a short pattern using that pad.
5. Save project, reload project, replay the pad and pattern.

Expected:
- Direct pad playback is unchanged from before this patch.
- Pattern playback is unchanged from before this patch.
- Save/load does not alter loudness.

## 2. Regression: Velocity Never Boosts Above Pad Level

Purpose: verify the attenuation-only rule.

Steps:
1. Put a sample on A1.
2. Set sample `LEVEL` to `40`.
3. Trigger A1 at normal UI velocity.
4. Later, when MIDI/debug velocity injection exists, trigger the same pad with:
   - velocity `127`
   - velocity `96`
   - velocity `64`
   - velocity `32`
5. Compare output loudness.

Expected:
- Velocity `127` matches the current pad loudness exactly.
- Lower velocities are quieter.
- No velocity value produces output louder than the `LEVEL=40` baseline.

## 3. Pattern Data Stores Velocity

Purpose: verify velocity survives persistence.

Steps:
1. Create a pattern with at least 4 hits on the same pad using different velocities.
2. Save project.
3. Save card image.
4. Reload project.
5. Reload card image.
6. Replay the pattern in both cases.

Expected:
- Relative loudness between hits is preserved.
- Event order is preserved.
- Missing `velocity` in older files should default to `127` on load.

## 4. Backward Compatibility

Purpose: old saves/cards should still load.

Steps:
1. Load an older project file with pattern events created before velocity existed.
2. Load an older card image created before velocity existed.
3. Replay patterns.

Expected:
- Load succeeds.
- Old events behave as `velocity=127`.
- No crashes, no silent patterns.

## 5. Future MIDI Input Test

Purpose: verify actual MIDI velocity mapping.

Steps:
1. Connect MIDI controller later.
2. Trigger the same pad from soft to hard repeatedly.
3. Repeat with sample `LEVEL=127`, then `LEVEL=80`, then `LEVEL=40`.
4. Record a pattern from MIDI.
5. Replay that pattern.

Expected:
- Harder hits are louder than softer hits.
- The loudest hit never exceeds the pad's current sample level.
- Recorded pattern playback reproduces the same relative loudness.

## 6. Voice-Level Independence

Purpose: verify retriggers do not overwrite each other’s velocity.

Steps:
1. Use a sample with a long tail.
2. Trigger it once at high velocity.
3. Retrigger it immediately at lower velocity.
4. If later possible, repeat with alternating low/high MIDI hits.

Expected:
- Each active voice keeps its own loudness.
- New retriggers do not retroactively change older voices.

## 7. Gate / Loop / Reverse Compatibility

Purpose: verify velocity does not break playback modes.

Steps:
1. Test one-shot playback with multiple velocities.
2. Test gate playback with multiple velocities.
3. Test loop playback with multiple velocities.
4. Test reverse playback with multiple velocities.
5. Test time-modified sample with multiple velocities.

Expected:
- Only loudness changes.
- Start/end/loop/reverse/time behavior remains unchanged.

## 8. Resample Compatibility

Purpose: verify attenuated playback is what gets resampled.

Steps:
1. Prepare two pads with the same sample.
2. Later, trigger one at higher velocity and one at lower velocity.
3. Resample the result to a new pad.
4. Replay the resampled pad.

Expected:
- Resample captures the actual mixed output loudness.
- Lower-velocity hits are quieter in the recorded result.

## Notes For Later

- Current UI does not yet expose variable per-hit velocity.
- Engine support is already in place:
  - per-voice velocity gain
  - pattern event velocity persistence
  - project/card load fallback to `127`
- When MIDI arrives, use MIDI velocity as trigger input and do not store it on the pad itself.
