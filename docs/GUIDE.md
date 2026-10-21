# Daybook setup and user guide

Daybook turns a jailbroken Kindle into a task, habit, goal, and journal dashboard. You can use touch controls on the Kindle and send commands from Telegram on your phone.

## 1. Before you start

The tested device is a **Kindle Basic 7th generation, WP63GW / KT2, firmware 5.12.2.2**, with a 600×800 portrait screen. Compatibility with other devices has not been established.

You need:

- An already-jailbroken Kindle that can launch `.sh` documents from the Library.
- FBInk available at `/var/local/kmc/bin/fbink`, with the launcher refresh helper at `/mnt/us/libkh/bin/fbink`.
- A USB data cable and a computer to transfer files.
- Wi-Fi and a Telegram account if you want phone control.
- `curl` with working HTTPS on the Kindle for Telegram. The tested device has curl 7.76.1.

This guide starts after jailbreaking. It does not provide a firmware image or a universal jailbreak procedure. Daybook keeps the existing Kindle system and starts manually from the Library.

Back up any existing `daybook/data` folder before updating. It contains tasks, journals, and private bot settings.

## 2. Install a release

Download and extract the latest ZIP from [GitHub Releases](https://github.com/DemiDemonDeveloper/kindle-daybook/releases). If you downloaded its checksum file too, macOS can verify it with `shasum -a 256 -c FILE.zip.sha256` from the download folder, replacing `FILE` with the actual filename.

Exit Daybook if it is already running, return to Kindle Home, and connect USB. The Kindle should show **USB Drive Mode** and appear as a drive on your computer.

Copy these files to the Kindle drive, creating the folders if needed:

| File in the ZIP | Location on the Kindle USB drive |
| --- | --- |
| `daybook/bin/daybook` | `daybook/bin/daybook` |
| `daybook/run-dashboard.sh` | `daybook/run-dashboard.sh` |
| `documents/Daybook.sh` | `documents/Daybook.sh` |
| `scripts/Check-Daybook.sh` (optional) | `documents/Check Daybook.sh` |

Copy the individual application files when updating. Preserve the existing `daybook/data` folder; do not replace it with an empty one.

Safely eject the drive, unplug the cable, and open **Daybook** in **Library → All items**. You should see Today, Daily, Goals, Progress, and a Journal button at the top.

### Install from source instead

Build as described in the [README](../README.md#build-and-test), then, with the Kindle connected at `/Volumes/Kindle`, run:

```sh
python3 scripts/install.py --volume /Volumes/Kindle
```

The installer backs up an existing installation under the project's ignored `build/backups` folder, copies the three application files, and verifies their hashes. Tasks and settings are preserved. Safely eject before launching.

## 3. Use the dashboard

| Area | How it behaves |
| --- | --- |
| **Today** | Unfinished to-dos carry over. Completed to-dos leave Today the next day. |
| **Daily** | Habits become available again each local day. Completion dates are retained. |
| **Goals** | Goals belong to their creation month. Older goals remain stored but leave the current-month view. |
| **Progress** | Shows a 14-day completion graph and seven-day habit grid. |
| **Journal** | Lists dated entries and lets you create, read, edit, or delete them. |

Use **+ Add** to create a task. Tap its checkbox to complete or reopen it. Tap **…** beside a task to rename or delete it; deletion on the Kindle requires confirmation. Use the page arrows for more items.

Open **Journal** to write a dated entry. Its editor supports multiple lines. Journal entries have their own IDs, separate from task IDs. Editing an entry preserves its original date.

Changes are kept in memory and saved to local storage before screen refreshes. Rendering does not reload the task file on every update.

To leave, choose **Menu → Exit dashboard**. Wait for Kindle Home before connecting USB.

## 4. Connect Telegram

Create a bot once, using your own Telegram account:

1. Open the verified **@BotFather** and send `/newbot`.
2. Follow the prompts for a bot name and username.
3. Keep the token BotFather gives you private.
4. Close Daybook and connect the Kindle by USB.
5. From the extracted release or source checkout, run:

```sh
python3 scripts/setup-telegram.py --data /Volumes/Kindle/daybook/data --mac-dialog
```

6. Paste the token into the hidden Mac prompt. On other systems, omit `--mac-dialog` to use a hidden terminal prompt, and adjust the drive path.
7. The helper displays your bot's username and a random `/start daybook_...` message. Send that exact message to your bot in a private chat within three minutes.
8. Wait for **Paired successfully**, safely eject, unplug USB, enable Wi-Fi, and open Daybook.
9. Send `/list` to your bot. Allow about 30 seconds for a reply.

The helper needs Python 3, curl, and internet access on the computer during pairing. The computer is not required afterward. Use a fresh bot dedicated to Daybook; another service polling the same bot can interfere with delivery.

Daybook accepts messages only from the paired private chat. **Keep the dashboard open and Wi-Fi enabled for sync.** The check utility is for troubleshooting; it does not keep syncing after it finishes.

The default local time zone is US Central with daylight saving rules. To use another zone, close Daybook and change the `timezone` value in `daybook/data/config.json` to a suitable POSIX time-zone rule. Reopen Daybook afterward. Local dates determine daily resets and monthly goal views.

## Telegram command reference

Replace example IDs with the IDs returned by the bot. Task IDs and journal IDs are separate and remain stable when another item is deleted.

| Command | Action |
| --- | --- |
| `/add Buy groceries` | Add a to-do. Plain text also adds a to-do. |
| `/daily Read for 20 minutes` | Add a daily habit. |
| `/goal Finish my portfolio` | Add a goal for the current month. |
| `/list` | List current tasks and their IDs. |
| `/habits` | Show habits. |
| `/progress` | Show progress. |
| `/done 3` | Mark task 3 complete for its applicable period. |
| `/undo 3` | Reopen task 3. |
| `/edit 3 Read for 30 minutes` | Change task 3's title. |
| `/delete 3` | Delete task 3 immediately. |
| `/journal Today I went for a walk.` | Add a dated journal entry. |
| `/journal` | List recent journal entry IDs. |
| `/jread 1` | Read journal entry 1 in Telegram. |
| `/jedit 1 Revised thoughts` | Replace entry 1's text, preserving its date. |
| `/jdelete 1` | Delete journal entry 1 immediately. |
| `/help` | Show command help. |

Telegram deletion commands do not show the Kindle's confirmation screen. Deleting a habit removes that habit's individual completion history; aggregate daily snapshots remain.

Journal entries typed only on the Kindle stay local unless requested through the bot. Text sent through Telegram is handled by Telegram as well as Daybook.

## 5. Back up and restore

Close Daybook before connecting USB. Copy the entire `daybook/data` folder to a private backup location on your computer.

| File | Purpose |
| --- | --- |
| `state.bin` | Current task, habit, progress, journal, and Telegram update-offset records. |
| `config.json` | Private bot token, paired chat, polling interval, and local time-zone settings. |
| `state.json` | Preserved older task data, if you upgraded from the initial version. |
| `history.log` | Activity log; journal bodies are omitted. |
| `launcher.log`, `launch.log` | Launch and connection diagnostics. |

On first launch, Daybook imports an existing `state.json` if no `state.bin` exists. The original JSON is preserved. Once the binary file exists, it is the active store.

To restore, close Daybook, connect USB, back up the current folder, and copy your saved `daybook/data` folder back. Safely eject and relaunch. Restore a coherent backup rather than mixing task files from different dates. Restoring an older update offset may allow retained Telegram commands to run again.

The data and token are not encrypted. Keep backups private and do not upload runtime files to GitHub.

## 6. Troubleshooting

### The bot does not reply

1. Confirm Daybook itself is open, Wi-Fi is enabled, and USB is disconnected.
2. Wait about 30 seconds or use the dashboard's sync action.
3. Confirm you are messaging your paired bot from the same private chat used during setup.
4. Make sure another app or service is not using that bot.
5. Exit Daybook and connect USB to inspect `daybook/data/launcher.log` and `launch.log`.

For a direct check, copy `scripts/Check-Daybook.sh` to `documents/Check Daybook.sh` on the Kindle. Safely eject, unplug, keep Wi-Fi enabled, and open **Check Daybook** in the Library. Allow about a minute. It checks the native executable, curl, public Telegram HTTPS, and one actual bot sync. It may apply queued commands and send replies. When it says **Daybook check saved**, reconnect USB and read `daybook/data/connection-check.log`.

The diagnostic log does not print the token. Other runtime request and response files can contain personal messages, so review files before sharing them.

### The Mac does not show the Kindle

Return to Kindle Home and reconnect the cable. Check that **USB Drive Mode** appears. Use a data-capable cable and another USB port if it only charges.

### The Library does not show Daybook

Confirm `Daybook.sh` is in the drive's `documents` folder, safely eject and disconnect, then check Library → All items. The jailbreak must support launching shell documents.

### Daybook freezes or will not exit

Use the normal Kindle restart menu. If that menu will not appear, hold Power for about 40 seconds and release to restart. Manual launch means the Kindle should return to its normal interface. Do not factory reset as a troubleshooting step. Reconnect USB after restarting and preserve the data folder before making changes.

### The date, habits, or monthly goals look wrong

Check the Kindle's date and time and Daybook's time-zone setting. Day changes control habits and progress; the current local month controls the Goals view.

### Some characters show as question marks

The current font and touch keyboard are ASCII. Other characters can be stored through Telegram but are displayed as `?` on the Kindle.

## 7. Remove Daybook

Exit the dashboard, connect USB, and back up `daybook/data`. Remove the `daybook` application folder, `documents/Daybook.sh`, and the optional `documents/Check Daybook.sh`. Safely eject. This removes Daybook, not the jailbreak or the Kindle operating system.

## 8. Add your project photos

See [the photo guide](photos/README.md). Use sample tasks and journal entries in pictures and keep credentials out of screenshots.
