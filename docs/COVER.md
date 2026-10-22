# Screen cover, wallpaper, and quotes

The screen cover hides the dashboard behind a wallpaper or a motivational quote. Briefly press and release the physical Power button to return to the screen you were using. Touches do nothing while covered, so bag touches cannot open the dashboard. This is a visual cover, not PIN protection or the Kindle's system sleep screen. The dashboard remains awake and Telegram keeps syncing, so battery use continues.

## Open the cover

Briefly press and release **Power** to lock the dashboard, and press it again to unlock. You can also choose **Menu → Lock screen now**. It also appears after five minutes without touch input. It does not interrupt an open task or journal editor, so an unfinished draft stays visible until you finish or open the cover manually.

In Menu, tap **Cover: quotes** or **Cover: wallpaper** to switch modes. The selection is saved. Touch cannot dismiss the cover. Button holds, key repeats, and rapid duplicate events do not toggle the cover. Gestures begun before or just after the unlock press are discarded; use a fresh touch afterward. The Power input is observed without grabbing it, leaving the system's long-press handling available. Unlocking the cover does not unlock the journal PIN.

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

A new wallpaper is selected automatically when no cover preference has been saved. The bottom strip displays the Power-button instruction. If the wallpaper is missing or invalid, the cover falls back to quotes.

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

Desktop checks cover the idle timer, protection of open editors, repeated-touch wake rejection, complete short button presses, long/repeated button rejection, gesture isolation, saved mode selection, custom quotes, and wallpaper validation. The physical KT2 reports KEY_POWER on its max77696-onkey input device; a short Power press to lock, touch rejection while covered, and a second short Power press to unlock have been verified on the device. The earlier quote cover was verified. Custom wallpaper loading and the automatic idle timer still need physical-device verification. The dashboard, exit, and Telegram journal flow have also been verified on the target KT2.

## If the button does not unlock

Use a short press and release, then pause a second before retrying. Tapping the screen cannot unlock this version. Daybook checks for a physical KEY_POWER input before launch; if none is available, it cancels startup and returns to the stock interface. See launcher.log. The optional Check-Power.sh utility records device/button events without changing journal data.
