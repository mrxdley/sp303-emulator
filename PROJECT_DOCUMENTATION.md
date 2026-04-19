# SP-303 Sampler Emulator

A C++ emulator/simulator for the Roland SP-303 sampler with a raylib-based GUI renderer.

## Project Overview

This project implements a functional software version of the Roland SP-303 drum machine/sampler, including:
- Full device model with 94 buttons, 4 knobs, 7-segment display, PEAK indicator
- Audio sampling from system input (microphone, line-in, or virtual audio)
- Sample playback with monophonic per-pad behavior
- Pattern-based workflow matching the original hardware

## Architecture

```
sp303/
├── core/               # Core device logic (C API)
│   ├── sp303.h        # Device state, button/knob enums, display helpers
│   └── sp303.cpp      # State machine, button handling, sampling logic
├── audio/              # Audio engine (miniaudio-based)
│   ├── sp303_audio.h  # Audio API, sample slots, recording
│   └── sp303_audio.cpp # Audio I/O, voice management, recording buffer
├── renderer/           # GUI application (raylib)
│   └── main.cpp       # Visual rendering, input handling, audio sync
├── build/              # Build artifacts (CMake)
└── *.sh               # Helper scripts for virtual audio setup
```

### Key Components

**Core Device (`core/sp303.cpp`)**
- Maintains full device state (buttons, knobs, display, bank selection)
- Implements sampling state machine: IDLE → STANDBY → RECORDING
- Handles pad LED blinking/solid states
- Manages 32-slot sample assignment tracking

**Audio Engine (`audio/sp303_audio.cpp`)**
- Stereo output playback, mono input capture
- 8-voice polyphony with per-slot monophonic behavior
- Ring buffer recording (60 seconds max per sample)
- Sample rate conversion and gain control

**Renderer (`renderer/main.cpp`)**
- Raylib-based visual interface
- Draggable UI elements (Shift+drag to reposition)
- TAB menu for audio configuration with live volume meter
- Keyboard keymap support

## Features Implemented

### Sampling Workflow (From Manual)
1. ✅ Press BANK button to select bank (A/B/C/D)
2. ✅ Press REC to enter sampling standby mode
3. ✅ All empty pads blink - user selects one
4. ✅ Selected pad goes solid, others off
5. ✅ Press REC again to start recording ("rEC" on display)
6. ✅ Press REC again to stop and save sample
7. ✅ Press pad to play recorded sample
8. ✅ PEAK indicator shows input level

### Display Features
- "---" in standby mode (pad selection needed)
- "rEC" while recording
- "FuL" when memory is full
- Numbers display with decimal points: `1.2.3.`
- Blank display in idle mode

### Audio Configuration (TAB Menu)
- Select output device
- Select input device
- Set sample rate (44100/48000/96000 Hz)
- Set buffer size (128-2048 frames)
- Live input volume meter with color gradient (green→yellow→red)

### Pad Behavior
- IDLE mode: No pads lit
- STANDBY mode: All empty pads blink until one is selected
- RECORDING mode: Selected pad lit solid, REC button blinks
- Playback: Monophonic per pad (retriggering cuts off previous play)

### Effect Architecture Notes
- `FILTER+DRIVE` is an insert effect processed per voice, per sample.
- `DELAY` is a bus effect processed on a wet bus after voices are mixed.
- `PITCH` is currently a deliberate simplification: it changes playback speed instead of using a hardware-faithful pitch algorithm.
- Because of that, `PITCH` changes both pitch and duration. This is intentional for now and may be replaced later.

## Virtual Audio Setup (Linux/PipeWire)

The project includes helper scripts for routing system audio (YouTube, etc.) into the sampler:

```bash
# Setup virtual input
./setup_virtual_input.sh

# Route all system audio through SP-303
./capture_system_audio.sh

# Restore normal audio
./restore_audio.sh

# Check current audio routing
./check_audio.sh
```

## Building

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Usage

```bash
./sp303_renderer
```

### Controls
- **Mouse**: Click buttons, drag knobs
- **Shift+drag**: Reposition UI elements
- **TAB**: Toggle audio config overlay
- **Keyboard**: Default keymap:
  - A,S,D,F,Z,X,C,V: Pads 1-8 (current bank)
  - 1,2,3,4: Bank A,B,C,D
  - Space: REC
  - Enter: Pattern Select
  - Backspace: DEL
  - Escape: CANCEL

## Technical Implementation Details

### Sampling State Machine
```
SAMPLING_IDLE
    ↓ [REC pressed]
SAMPLING_STANDBY
    ↓ [Pad pressed] ← User selects target
    ↓ [REC pressed]
SAMPLING_RECORDING
    ↓ [REC pressed] or [FuL]
    → Assign to pad → IDLE
```

### Audio Flow
1. Capture callback receives stereo input
2. Mix to mono with gain (CTRL 3/MFX knob)
3. If recording active: write to ring buffer
4. Update input_peak meter (for PEAK indicator)
5. Playback callback mixes active voices to stereo output

### Sample Storage
- Ring buffer: 60 seconds max (at 48kHz)
- Extraction on stop: handles wrapped/non-wrapped data
- Assignment to slot under mutex protection

### Key Design Decisions

**1. Last Sampling Target Pad**
Problem: Core clears `sampling_target_pad` when exiting RECORDING, but renderer needs it for audio assignment.
Solution: Added `last_sampling_target_pad` field that persists until next recording.

**2. Input Gain Default**
DRIVE knob (CTRL 3/MFX) controls input gain. Starts at 0.8 (80%) for reasonable recording levels.

**3. No Pads Lit in IDLE**
Intentional design choice - pads only light during sampling modes.

**4. Monophonic Per Pad**
Pressing same pad twice retriggers it instead of stacking voices. Implemented by checking if slot already has active voice.

## Known Issues / Limitations

1. **Audio artifacts/crackling**: May occur with small buffer sizes. Try increasing to 1024 or 2048 frames in TAB config.

2. **PEAK indicator threshold**: Set to 0.3 (30% input level). May need tuning based on input source.

3. **Virtual audio setup**: Linux-specific scripts provided. Other platforms need manual audio routing.

4. **No pattern sequencing yet**: Core state machine supports it, but not implemented in renderer.

5. **No DSP effects**: FILTER, PITCH, DELAY, etc. buttons are UI only (no audio processing).

## File Structure

```
sp303/
├── CMakeLists.txt           # Build configuration
├── setup_virtual_input.sh   # Create virtual audio input
├── capture_system_audio.sh  # Route system audio to SP-303
├── restore_audio.sh         # Restore normal audio
├── check_audio.sh           # Debug audio routing
├── test_tone.sh             # Generate test tone
├── test_recording.sh        # Automated recording test
├── core/
│   ├── sp303.h             # C API header
│   └── sp303.cpp           # Core implementation (~450 lines)
├── audio/
│   ├── sp303_audio.h       # Audio API header
│   └── sp303_audio.cpp     # Audio implementation (~475 lines)
├── renderer/
│   └── main.cpp            # GUI renderer (~940 lines)
├── tests/
│   └── smoke.cpp           # Basic unit tests
└── build/                  # CMake build directory
```

## Dependencies

- **raylib 5.5**: Graphics, windowing
- **miniaudio 0.11.21**: Audio I/O, device enumeration
- **nlohmann/json 3.11.3**: Configuration file handling
- **CMake 3.20+**: Build system
- **C++20**: Language standard

## Development History

This project was developed incrementally with the following major milestones:

1. **Initial Setup**: CMake build, raylib window, basic button layout
2. **Core Device**: State management, button enums, display encoding
3. **Audio Engine**: Playback voices, sample loading, device enumeration
4. **Recording**: Ring buffer, gain control, assignment logic
5. **Sampling Workflow**: State machine (IDLE→STANDBY→RECORDING)
6. **Visual Polish**: LED blinking, drag-to-reposition, TAB config screen
7. **Virtual Audio**: PipeWire integration for system audio routing
8. **Monophonic**: Per-pad retrigger behavior
9. **Display Features**: Decimal points, "rEC" indicator

## Future Enhancements

- Pattern sequencer implementation
- DSP effects (filter, pitch, delay)
- Sample import/export (WAV files)
- Save/load projects
- MIDI input support
- Android build support (CMake already configured)

## License

See repository for license information.

---

**Last Updated**: April 2026  
**Status**: Functional sampling workflow complete, basic playback working
