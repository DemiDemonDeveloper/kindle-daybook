# Architecture

- `src/model.c`: tasks, stable IDs, daily habit dates, progress snapshots, command handling, JSON validation, atomic persistence.
- `src/daybook.c`: 600×800 bitmap rendering, touch input, navigation, Telegram polling and owner checks.
- `scripts/Daybook.sh`: Library entry point.
- `scripts/run-dashboard.sh`: probe, suspend stock interface, supervise dashboard, restore interface on exit.
- `scripts/setup-telegram.py`: hidden-token bot validation and private-chat pairing.
- `tests/`: model semantics, touch flows, authorization, duplicate Telegram updates, and history persistence.

The ARM executable is statically linked against musl. FBInk refreshes the framebuffer from an on-device PGM image; it is not bundled. Touch ranges are read from the Linux input interface instead of hard-coding raw coordinates. Initial support is portrait only.

The UI and Telegram use the same model and command path. Network requests run in child processes so they do not block touch handling. Applied Telegram update IDs are saved atomically with task changes; replayed updates cannot duplicate commands. Failed replies may be lost, but committed task changes remain. A file lock prevents concurrent app instances from changing the same state.

`state.json` is authoritative. Saves use a temporary file, fsync, and rename. A malformed existing file causes startup to fail instead of silently replacing it. `history.jsonl` is an append-only activity record; it is not used as a second source of truth. `config.json` contains the paired chat, secret bot token, polling interval, and POSIX time-zone string. The default is America/Chicago's current DST rules.

Daily routines store their last 90 completed dates. The model also stores the last 90 recorded daily snapshots: completed/total visible tasks and habits. Missing days are shown as no data instead of invented zeroes. A daily snapshot is recorded on launch, at midnight while running, and after edits. Past snapshots are not rewritten as the task list evolves.

## Roadmap

1. Validate touchscreen, native launch/exit, USB recovery, and persistence on the physical KT2.
2. Pair Telegram and verify authorized add/done/delete and network recovery.
3. Improve sleep/wake and polling battery use.
4. Add opt-in dedicated startup, a USB bypass, and rollback controls.
5. Consider a persistent dashboard on the sleep screen and richer habit statistics.

Replacing the bootloader/kernel or flashing another device's firmware is outside this first implementation.
