#!/bin/sh
# Name: Check Daybook
# Description: Check Daybook and HTTPS without reading bot credentials.
APP=/mnt/us/daybook
LOG="$APP/data/connection-check.log"
mkdir -p "$APP/data"
{
 echo 'Daybook connection check v1'
 date -u
 echo 'Executable'
 ls -l "$APP/bin/daybook"
 echo 'Native probe'
 "$APP/bin/daybook" --data "$APP/data" --probe
 echo "Probe status: $?"
 echo 'Network tool'
 command -v curl
 curl --version
 echo 'Additional curl locations'
 for p in /usr/bin/curl /var/local/kmc/bin/curl /mnt/us/libkh/bin/curl; do
  if [ -x "$p" ]; then ls -l "$p"; fi
 done
 echo 'Public Telegram HTTPS check (no bot token)'
 curl --silent --show-error --connect-timeout 8 --max-time 20 --output /dev/null --write-out 'HTTP status: %{http_code}\n' https://api.telegram.org/
 echo "HTTPS status: $?"
 echo 'Direct Daybook Telegram sync'
 "$APP/bin/daybook" --data "$APP/data" --sync-once
 echo "Sync status: $?"
 echo 'Check complete' 
} > "$LOG" 2>&1
eips 0 4 'Daybook check saved. Reconnect USB.'
