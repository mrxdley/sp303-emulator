# TODO

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
