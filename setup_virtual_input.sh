#!/bin/bash
# SP-303 Virtual Audio Input Setup - PROPER VERSION
# Routes system audio to SP-303 WITHOUT breaking your speakers

echo "Setting up virtual audio input for SP-303..."

# Check if already set up
if pactl list | grep -q "sp303_input"; then
    echo "SP-303 virtual input already exists"
    exit 0
fi

# Create a null sink that SP-303 will read from
pactl load-module module-null-sink sink_name=sp303_input sink_properties=device.description="SP303_Input" rate=44100

# Create a loopback from the REAL output's monitor to our virtual sink
# This copies audio without breaking the main output
REAL_SINK=$(pactl info | grep "Default Sink" | cut -d: -f2 | xargs)
if [ -z "$REAL_SINK" ]; then
    REAL_SINK="alsa_output.pci-0000_05_00.6.analog-stereo"
fi

pactl load-module module-loopback source="${REAL_SINK}.monitor" sink=sp303_input latency_msec=5

echo ""
echo "✓ Virtual audio input created!"
echo ""
echo "SP-303 will auto-detect 'sp303_input.monitor' as input"
echo "Your speakers still work normally"
echo ""
echo "To remove this later, run: ./remove_virtual_input.sh"
