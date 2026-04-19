#!/bin/bash
# Show current audio routing

echo "=== Current Audio Setup ==="
echo ""
echo "Default Output:"
pactl info | grep "Default Sink"
echo ""
echo "Available Inputs for SP-303:"
pactl list sources | grep -E "Name:|Description:" | grep -v "\.monitor" | head -10
echo ""
echo "Virtual Devices:"
pactl list | grep -E "Name:|Description:" | grep "sp303" || echo "  (none)"
