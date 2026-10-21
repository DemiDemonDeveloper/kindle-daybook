# Architecture

- `src/model.c`: tasks, stable IDs, daily habit dates, progress snapshots, command handling, one-time legacy JSON import.
- `src/store.c`: validated binary records, length-prefixed strings, checksum, atomic persistence.
- `src/daybook.c`: 600×800 bitmap rendering, touch input, navigation, Telegram polling and owner checks.
- `scripts/Daybook.sh`: Library entry point.
- `scripts/run-dashboard.sh`: probe, pause the stock window manager, supervise dashboard, restore its framebuffer and resume it on exit. The framework is never stopped.
- `scripts/setup-telegram.py`: hidden-token bot validation and private-chat pairing.
- `tests/`: model semantics, touch flows, authorization, duplicate Telegram updates, and history persistence.

The ARM executable is statically linked against musl. FBInk refreshes the framebuffer from an on-device PGM image; it is not bundled. Touch ranges are read from the Linux input interface instead of hard-coding raw coordinates. Initial support is portrait only.

The UI and Telegram use the same model and command path. Network requests run in child processes so they do not block touch handling. Applied Telegram update IDs are saved atomically with task changes; replayed updates cannot duplicate commands. Failed replies may be lost, but committed task changes remain. A file lock prevents concurrent app instances from changing the same state.

`state.bin` is authoritative. Its versioned, little-endian records and strings have explicit lengths and a checksum; it is portable between the host and ARM builds. The C model is loaded once at startup and kept in memory. Saves use a temporary file, fsync, and rename, both on data changes and before display refreshes. A malformed file causes startup to fail rather than being silently replaced. The old `state.json` is imported once when no binary store exists, preserving tasks, IDs, habit history, graph snapshots, and Telegram offset. `history.log` is a plain-text activity record; journal bodies are excluded. `config.json` contains the paired chat, secret bot token, polling interval, and POSIX time-zone string. The default is America/Chicago's current DST rules.

Daily routines store their last 90 completed dates. The model also stores the last 90 recorded daily snapshots: completed/total visible tasks and habits. Missing days are shown as no data instead of invented zeroes. A daily snapshot is recorded on launch, at midnight while running, and after edits. Past snapshots are not rewritten as the task list evolves.

## Roadmap

1. Task controls, native launch, and the revised pause/resume exit have passed physical KT2 checks. Verify journal and binary migration next.
2. Pair Telegram and verify authorized add/done/delete and network recovery.
3. Improve sleep/wake and polling battery use.
4. Add opt-in dedicated startup, a USB bypass, and rollback controls.
5. Consider a persistent dashboard on the sleep screen and richer habit statistics.

Journal entries have separate stable IDs, original creation dates, and multiline bodies. Editing an entry retains its date. Kindle deletion requires confirmation. The local binary format and Telegram transport do not encrypt data at rest; protect device access and backups.

Replacing the bootloader/kernel or flashing another device's firmware is outside this first implementation.
