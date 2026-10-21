#!/bin/sh
set -u
APP=/mnt/us/daybook
mkdir -p "$APP/data"
exec >> "$APP/data/launcher.log" 2>&1
LOCK="$APP/data/launcher-lock"
if ! mkdir "$LOCK" 2>/dev/null; then
 old=$(cat "$LOCK/pid" 2>/dev/null || true)
 case "$old" in ''|*[!0-9]*) exit 1 ;; esac
 if kill -0 "$old" 2>/dev/null; then exit 0; fi
 rm -f "$LOCK/pid"
 rmdir "$LOCK" || exit 1
 mkdir "$LOCK" || exit 1
fi
echo "$$" > "$LOCK/pid"
child=""
wm_pids=""
pillow_disabled=0
snapshot="$APP/data/stock-screen.dump"
cleanup() {
 trap - EXIT INT TERM HUP
 if [ -n "$child" ]; then kill "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
 # Restore pixels before resuming the stock window manager, as KOReader does.
 if [ -s "$snapshot" ]; then cat "$snapshot" > /dev/fb0 2>/dev/null || true; rm -f "$snapshot"; fi
 for pid in $wm_pids; do kill -CONT "$pid" 2>/dev/null || true; done
 if [ "$pillow_disabled" = 1 ]; then lipc-set-prop com.lab126.pillow disableEnablePillow enable 2>/dev/null || true; fi
 lipc-set-prop com.lab126.powerd preventScreenSaver 0 2>/dev/null || true
 lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home 2>/dev/null || true
 "$APP/../libkh/bin/fbink" -q -s 2>/dev/null || true
 rm -f "$LOCK/pid"
 rmdir "$LOCK" 2>/dev/null || true
 echo "Daybook exit: stock interface resumed; framework was never stopped"
}
trap cleanup EXIT INT TERM HUP
if ! "$APP/bin/daybook" --data "$APP/data" --probe; then
 eips 0 5 "Daybook needs attention. Reconnect USB."
 exit 1
fi
# Never stop or restart the Amazon framework. It retains its live application state.
if ! cat /dev/fb0 > "$snapshot" || [ ! -s "$snapshot" ]; then
 echo "Could not preserve stock framebuffer; launch cancelled"
 exit 1
fi
if lipc-set-prop com.lab126.pillow disableEnablePillow disable; then pillow_disabled=1; fi
pids=$(pidof awesome 2>/dev/null || true)
if [ -z "$pids" ]; then echo "Window manager not found; launch cancelled"; exit 1; fi
for pid in $pids; do
 if kill -STOP "$pid"; then wm_pids="$wm_pids $pid"; else exit 1; fi
done
lipc-set-prop com.lab126.powerd preventScreenSaver 1 2>/dev/null || true
echo "Daybook launch: framework preserved; window manager paused"
"$APP/bin/daybook" --data "$APP/data" &
child=$!
wait "$child"
result=$?
child=""
exit "$result"
