# Kindle Daybook

A dedicated, native task dashboard for a jailbroken **Kindle Basic 7th generation (WP63GW / KT2), firmware 5.12.2.2**.

This is a custom interface running on the existing Kindle Linux system, **not a replacement firmware image**. Development starts with a manually launched alpha. Boot-time replacement waits for successful device testing.

## First version

- Today: unfinished to-dos carry over; completed to-dos leave Today the next day.
- Daily routines reset at local midnight and retain up to 90 completion dates per habit.
- Monthly goals stay associated with their creation month.
- Touch controls to add, rename, complete, reopen, and delete tasks. Deletion requires confirmation on the Kindle.
- Progress: 14-day completion graph and a seven-day habit grid, backed by saved history.
- Direct Telegram bot integration: add, edit, list, delete, complete, and undo tasks. No always-on Mac or separate hosting server is needed while the dashboard runs and the Kindle has Wi-Fi.
- Local C records and strings in a checksummed, versioned binary file, saved atomically before screen refreshes. Existing task JSON is imported once; screen rendering reads memory.
- A dated journal with multiline entries, editing, confirmed deletion, and Telegram commands.

## Status

**0.2.0 alpha — task controls and pause/resume exit have been tested on the physical KT2. Journal/storage migration and Telegram need verification on the device.** Telegram needs your own bot, created through `@BotFather`. No credentials are included in this repository.

![Progress preview using sample data](docs/progress-preview.png)

![Journal preview using sample data](docs/journal-preview.png)

See [Installation](docs/INSTALL.md), [Telegram setup](docs/TELEGRAM.md), and [Architecture](docs/ARCHITECTURE.md).

## Build

```sh
make test host
ZIG=/path/to/zig make kindle
```

The ARM build uses Zig 0.17.0 as a cross-compiler for `arm-linux-musleabi`, Cortex-A9. The resulting executable is statically linked. The Kindle uses its installed FBInk helper for e-ink refreshes.

Desktop preview using the same renderer:

```sh
./build/daybook-host --data build/preview-data --demo --snapshot build/preview.pgm
```

Task files, tokens, request/response caches, logs, and runtime data are ignored by Git. Keep your token out of issues, screenshots, and commits.

## Deliberate alpha limits

- 600×800 portrait layout, matching the target Kindle.
- Up to 256 tasks, 90 daily progress snapshots, 90 completed dates per habit, and 128 journal entries of up to 2048 bytes each.
- Initial display font and on-screen keyboard use ASCII; other characters are stored but displayed as `?`.
- The dashboard stays awake while active. Exit from Menu before connecting USB. Telegram sync pauses when the app is closed or the Kindle is offline.
- No automatic startup, firmware flashing, reminders, arbitrary natural-language date parsing, or background sync while asleep in this release.

## License

MIT for original project code. cJSON 1.7.19 is MIT licensed; the font8x8 basic bitmap font is public domain. Their original notices are retained in `vendor/`. FBInk is a separately installed dependency, not bundled here.
