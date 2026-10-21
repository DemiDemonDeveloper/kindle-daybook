# Install and test the alpha

Target: jailbroken Kindle Basic 7th generation (WP63GW / KT2), firmware 5.12.2.2, portrait 600×800. The jailbreak must already support `.sh` documents and have FBInk installed at `/var/local/kmc/bin/fbink`.

This package runs a custom native interface. It does not flash firmware or change startup behavior.

1. Build with `ZIG=/path/to/zig make kindle`.
2. Keep the Kindle on its normal screen and connect it by USB.
3. On the Mac, run `python3 scripts/install.py --volume /Volumes/Kindle`.
4. Safely eject the Kindle and unplug the cable.
5. In Library → All items, open **Daybook** once. Choose and confirm a 6–12 digit journal PIN on the device; keep it safe because there is no recovery for a lost PIN. See [Journal privacy](JOURNAL-LOCK.md).
6. Add a task, mark it complete, reopen it, and check the Progress tab. Open Journal in the header to write and read a dated entry.
7. Use Menu → Exit dashboard before reconnecting USB. Verify that your task survives reopening the app.

The launcher checks the executable and touchscreen, preserves the current framebuffer, and pauses the stock window manager. It keeps the framework alive, then restores the screen, resumes the window manager, and opens Home on exit. It also restores normal screen-saver settings. If launching fails, reconnect USB and inspect `daybook/data/launch.log` and `launcher.log`. Task and journal data live in `daybook/data/state.bin`. On the first 0.2 launch, an existing `state.json` is imported while the original is preserved.

## Recovery

Automatic startup is not enabled in this alpha. If the dashboard is unresponsive, use the Kindle's normal restart mechanism; it should return to the stock interface. Do not use factory reset. If the application has exited, deleting `documents/Daybook.sh` and the `daybook` folder removes this application; back up `daybook/data` first to preserve tasks and bot settings. This does not remove the jailbreak.

## Power and connectivity

The first version keeps the Kindle awake while the dashboard is active, allowing touch input and Telegram polling. Leaving Wi-Fi active affects battery life. Exit the dashboard when charging or transferring files. There is no sync while it is closed or offline; Telegram retains pending updates for a limited time, so reopen it regularly.

## Later: dedicated startup

After manual launch, exit/recovery, USB handling, and battery behavior pass on-device testing, we can add an opt-in startup service with a USB maintenance bypass. No startup service is installed by the current package.
