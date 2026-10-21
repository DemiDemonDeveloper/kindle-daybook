# Screen cover, wallpaper, and quotes

The screen cover hides the dashboard behind a wallpaper or a motivational quote. Tap once to return to the screen you were using. This is a visual cover, not PIN protection or the Kindle's system sleep screen. The dashboard remains awake and Telegram keeps syncing, so battery use continues.

## Open the cover

Choose **Menu → Lock screen now**. It also appears after five minutes without touch input. It does not interrupt an open task or journal editor, so an unfinished draft stays visible until you finish or open the cover manually.

In Menu, tap **Cover: quotes** or **Cover: wallpaper** to switch modes. The selection is saved. The first tap on the cover dismisses it without activating a control underneath.

## Custom wallpaper

1. On your computer, choose a local photo. Install Pillow if needed: `python3 -m pip install Pillow`.
2. From the source checkout or extracted release, convert it:

```sh
python3 scripts/prepare-wallpaper.py my-photo.jpg wallpaper.pgm
```

The helper preserves the original, applies its orientation, crops it to fill 600×800, and converts it to grayscale. It refuses to overwrite an existing output file.

3. Exit Daybook and connect the Kindle by USB.
4. Copy the output to **`daybook/data/wallpaper.pgm`** on the Kindle.
5. Safely eject, unplug, and open Daybook.
6. Select **Menu → Cover: wallpaper**, then **Lock screen now**.

A new wallpaper is selected automatically when no cover preference has been saved. The bottom strip displays the tap-to-return instruction. If the wallpaper is missing or invalid, the cover falls back to quotes.

## Custom quotes

While Daybook is closed and USB is connected, create **`daybook/data/quotes.txt`**, with one quote per line. For example:

```text
One small step today is still progress.
Make room for what matters today.
Pause. Breathe. Begin again.
```

Up to 32 nonempty lines are used, with a maximum of 160 bytes per line. Use ASCII for the current Kindle font. Select quote mode in Menu. Each time the cover opens, it advances to another quote. Without a custom file, Daybook uses its built-in phrases. It does not refresh constantly while covered.

## Idle delay

The default is 300 seconds. Advanced configuration: add `"idle_seconds": 600` to the existing `daybook/data/config.json` object for ten minutes, or `"idle_seconds": 0` to disable automatic covering. Values from 30 through 86400 seconds are accepted. Preserve your other settings and keep the bot token private. Restart Daybook after changing settings.

Cover mode is saved separately in `daybook/data/cover-mode.txt`. Include that file, `quotes.txt`, and `wallpaper.pgm` in your private data backup.

## Verification status

Desktop checks cover the idle timer, protection of open editors, wake-tap isolation, saved mode selection, custom quotes, and wallpaper validation. Physical Kindle verification is pending for this new feature. The earlier dashboard, exit, and Telegram journal flow have already been verified on the target KT2.
