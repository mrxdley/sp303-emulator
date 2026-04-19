#!/bin/bash
# Route browser/system audio to SP-303 for sampling

echo "Routing ALL system audio to SP-303 virtual input..."
pactl set-default-sink sp303_capture

echo ""
echo "✓ Done! All audio now goes through SP-303_Capture"
echo ""
echo "Open SP-303, press TAB, and select 'Monitor of SP-303_Capture' as input"
echo ""
echo "To restore normal audio, run: ./restore_audio.sh"
