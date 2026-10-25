# BattleBuddy: LilyGO T-Display-S3 Touch prototype

A single-player counter running on the T-Display-S3 Touch (1.9" 170×320 LCD,
CST816 capacitive touch, ESP32-S3). The counter logic lives in
[`core/`](../../core) and is shared with future boards.

![UI preview](snapshot/preview.png)

## Controls

Hold the board landscape.

| Input | Action |
|---|---|
| Tap left third | −1 (or −5) |
| Tap right third | +1 (or +5) |
| Hold left/right | Repeat |
| Tap middle | Switch step between 1 and 5 |
| KEY button (GPIO14) | Undo |
| BOOT button (GPIO0), short | Next game (Riftbound → Magic → Commander → Lorcana → Flesh and Blood) |
| BOOT button, hold 1 s | Restart the current game |

Quick taps are grouped: changes within 1.5 s of each other show as a pending
`+3` / `-2` under the number, then become one history entry along the bottom.
Undo removes the pending change first, then history entries one by one.

The screen dims after 30 s; the first touch or button press only wakes it.

## Build and flash

Needs [PlatformIO](https://platformio.org/install) (VS Code extension or `pip install platformio`).

```sh
cd firmware/tdisplay-s3
pio run -t upload
pio device monitor   # optional serial log
```

If the upload can't find the board, hold BOOT, press and release RST, then
release BOOT and run the upload again.

## Things to check on first boot

These were set from LilyGO's pin map but haven't been tested on hardware yet:

- **Orientation:** if the picture is upside down, change `lcd.setRotation(1)`
  to `3` in `src/main.cpp`.
- **Touch alignment:** if taps land on the wrong side, adjust
  `offset_rotation` in `src/display.h` (try 1, 2, 3).
- **Battery reading:** the top-right shows `USB` above 4.3 V, otherwise a rough
  percentage. Plug in a LiPo and compare.

## Previewing the UI without hardware

`snapshot/` renders the screen for a few game states using the same drawing
code, on a Linux/macOS host:

```sh
cd firmware/tdisplay-s3
pio run                 # downloads LovyanGFX into .pio/
make -C snapshot        # writes PPM images to snapshot/build/
```
