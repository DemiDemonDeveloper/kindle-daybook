#!/bin/sh
# Name: Check Power Button
# Description: Capture Power-button events without changing Daybook data.
APP=/mnt/us/daybook
OUT="$APP/data/power-check"
mkdir -p "$OUT"
pids=""
old=$(lipc-get-prop com.lab126.powerd preventScreenSaver 2>/dev/null || echo 0)
cleanup() {
 trap - EXIT INT TERM HUP
 for pid in $pids; do kill "$pid" 2>/dev/null || true; done
 for pid in $pids; do wait "$pid" 2>/dev/null || true; done
 case "$old" in 0|1) lipc-set-prop com.lab126.powerd preventScreenSaver "$old" 2>/dev/null || true ;; esac
}
trap cleanup EXIT INT TERM HUP
cat /proc/bus/input/devices > "$OUT/devices.txt"
lipc-probe -v com.lab126.powerd > "$OUT/powerd.txt" 2>&1
lipc-set-prop com.lab126.powerd preventScreenSaver 1 2>/dev/null
for event in /dev/input/event*; do
 [ -e "$event" ] || continue
 cat "$event" > "$OUT/${event##*/}.bin" 2>/dev/null &
 pids="$pids $!"
done
lipc-wait-event -m -s 0 com.lab126.powerd '*' > "$OUT/events.txt" 2>&1 &
pids="$pids $!"
eips 0 3 'Press Power twice, briefly.'
eips 0 5 'Wait 3 seconds between presses.'
eips 0 7 'Check finishes in 30 seconds.'
sleep 30
cleanup
eips 0 9 'Button check saved. Reconnect USB.'
