# Audio Loopback How-To

This emulator can resample its own output internally, but if you want to sample desktop audio through `EXT SOURCE` on Linux, you need a loopback route from system output into the emulator input.

## PipeWire / PulseAudio

Recommended tools:
- `pavucontrol`
- `qpwgraph` or `helvum`

### Easiest Path

1. Open the emulator.
2. Open `IN-LINE-OUT`.
3. Set the emulator input device to your loopback/monitor source.
4. In `pavucontrol`:
   - Playback tab: make sure the source app is going to the output you expect.
   - Recording tab: set the emulator capture stream to the monitor/loopback source.

Typical monitor source names:
- `Monitor of Built-in Audio Analog Stereo`
- `Monitor of <your output device>`

### PipeWire Graph Route

If the monitor source is not obvious:

1. Open `qpwgraph` or `helvum`.
2. Find the desktop audio source you want to sample.
3. Find the emulator capture/input node.
4. Connect source output -> emulator input.

### Notes

- If the emulator records silence, the input device is usually wrong.
- If feedback happens, you routed the emulator output back into itself while monitoring live.
- For clean sampling, use a monitor source, not your microphone.

## JACK-style Setups

If you already use JACK/PipeWire patching:

1. Set emulator input to the JACK/PipeWire capture device you want.
2. Patch the source app or bus into the emulator input ports.
3. Keep emulator output routed separately to avoid a feedback loop.

## What We Should Improve Later

- clearer device naming in the menu
- a dedicated “desktop loopback” help hint in `IN-LINE-OUT`
- maybe a one-screen walkthrough for PipeWire users
