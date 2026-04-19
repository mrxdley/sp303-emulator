#!/bin/bash
# Play test tone to verify audio routing

echo "Playing 440Hz test tone for 5 seconds..."
echo "You should see the volume meter move in SP-303 config screen"

# Use speaker-test to generate a tone
speaker-test -t sine -f 440 -c 2 -s 1 &
PID=$!

sleep 5
kill $PID 2>/dev/null

echo "Test complete"
