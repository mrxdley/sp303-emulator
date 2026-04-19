#!/bin/bash
# Remove SP-303 virtual audio input

echo "Removing SP-303 virtual audio..."

# Unload loopback modules that point to sp303_input
for MODULE in $(pactl list modules short | grep -E "module-loopback|module-null-sink" | grep "sp303" | awk '{print $1}'); do
    pactl unload-module "$MODULE" 2>/dev/null
done

echo "✓ Virtual audio removed"
