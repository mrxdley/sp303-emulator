#!/bin/bash
# Restore normal audio output

echo "Restoring default audio output..."
# Find the first real audio sink (not our virtual one)
DEFAULT_SINK=$(pactl list sinks | grep -E "Name:" | grep -v "sp303_capture" | head -1 | sed 's/.*Name: //')
pactl set-default-sink "$DEFAULT_SINK"
echo "✓ Audio restored to: $DEFAULT_SINK"
