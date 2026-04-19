#!/bin/bash
# Test SP-303 recording and playback

cd /home/mrxdley/sp303/build

echo "Starting SP-303 test..."
echo "1. Wait for window to open"
echo "2. Press BANK B, then REC twice to record"
echo "3. Wait 3 seconds, press REC to stop"
echo "4. Press the pad to play back"
echo "5. Close window to see debug output"
echo ""

./sp303_renderer 2>&1 | tee /tmp/sp303_test.log | grep -E "^\[SP-303\]|^\[ASSIGN\]|^\[TRIGGER\]" 

echo ""
echo "=== Full log ==="
cat /tmp/sp303_test.log | grep -E "^\[SP-303\]|^\[ASSIGN\]|^\[TRIGGER\]|^\[AUDIO\]"
