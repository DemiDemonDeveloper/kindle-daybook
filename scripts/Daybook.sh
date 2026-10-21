#!/bin/sh
# Name: Daybook
# Description: Daily tasks, monthly goals, habits, and progress.
APP=/mnt/us/daybook
if [ ! -x "$APP/bin/daybook" ]; then
 eips 0 5 "Daybook is not installed."
 exit 1
fi
# Detach from the Library launcher before temporarily suspending the Kindle UI.
nohup sh "$APP/run-dashboard.sh" > "$APP/data/launch.log" 2>&1 < /dev/null &
exit 0
