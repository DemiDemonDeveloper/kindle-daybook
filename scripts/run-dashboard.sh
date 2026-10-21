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
trap 'rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null || true' EXIT
if ! "$APP/bin/daybook" --data "$APP/data" --probe; then
 eips 0 5 "Daybook needs attention. Reconnect USB."
 exit 1
fi
# Probe before suspending the stock interface. A normal exit restores it.
framework_was_running=0
if status framework 2>/dev/null | grep -q running; then framework_was_running=1; fi
child=""
cleanup() {
 trap - EXIT INT TERM HUP
 rm -f "$LOCK/pid"
 rmdir "$LOCK" 2>/dev/null || true
 if [ -n "$child" ]; then kill "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
 lipc-set-prop com.lab126.powerd preventScreenSaver 0 2>/dev/null || true
 if [ "$framework_was_running" = 1 ]; then start framework 2>/dev/null || true; fi
}
trap cleanup EXIT INT TERM HUP
lipc-set-prop com.lab126.powerd preventScreenSaver 1 2>/dev/null || true
if [ "$framework_was_running" = 1 ]; then stop framework || exit 1; fi
"$APP/bin/daybook" --data "$APP/data" &
child=$!
wait "$child"
result=$?
child=""
exit "$result"
