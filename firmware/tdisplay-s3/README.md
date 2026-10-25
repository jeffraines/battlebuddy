# BattleBuddy: LilyGO T-Display-S3 Touch prototype

A single-player counter running on the T-Display-S3 Touch (1.9" 170×320 LCD,
CST816 capacitive touch, ESP32-S3). The counter logic lives in
[`core/`](../../core) and is shared with future boards.

![UI preview](snapshot/preview.png)

## Controls

Hold the board landscape, USB port on the right.

| Input | Action |
|---|---|
| **BOOT** button, short press | Open / close the menu |
| **BOOT**, hold 1 s | Sleep |
| **KEY** button | Undo (shows what was undone); closes the menu if open; wakes from sleep |
| Circle below the screen | Open / close the menu |

**Menu:** tap a tile; tap the header to close.

| Tile | Does |
|---|---|
| Game | Cycles Riftbound → Magic → Commander → Lorcana → Flesh and Blood (restarts) |
| Players | *Just you* or *Both* on one screen (restarts) |
| Layout | Two players only: *YOU left*, *YOU right*, or *Facing* (YOU along the bottom, OPP along the top upside down, for a device lying between you) |
| Flip | Rotates the whole screen 180° (USB port on the left) |
| Restart | New game |
| Sleep | Sleep now; KEY wakes |

**Counting:**

- Riftbound: tap **CONQUER** or **HOLD** to score a point of that kind. Every
  point is recorded on its own, right away. A row of 8 slots per player fills
  with **C** / **H** in the order the points were scored, so you can see
  exactly how each point was earned. Mistakes are fixed with undo.
- Other games, one player: left third −, right third +, middle switches the
  step between 1 and 5. Hold to repeat.
- Other games, two players: side by side, top half of a player's side +1,
  bottom half −1; facing, left third −1, right third +1 of your own strip.

For games other than Riftbound, quick taps are grouped: changes within 1.5 s
of each other show as a pending `+3` / `-2`, then become one history entry
along the bottom (colored by player in two-player mode).

Each game has its own color theme. The screen dims after 30 s idle and the
board sleeps after 10 minutes; the first touch or press after dimming only
wakes the screen. The game survives sleep, but not a reset or power cut.

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
- **Battery reading:** the top-right shows a lightning bolt above 4.3 V (USB),
  otherwise a rough charge level. Plug in a LiPo and compare.
- **Circle key:** the serial monitor prints `touch raw x=.. y=..` for every
  touch. The circle should report y ≥ 320; if it reports something else, change
  `kRawHomeKeyY` in `src/main.cpp`.

## Previewing the UI without hardware

`snapshot/` renders the screen for a few game states using the same drawing
code, on a Linux/macOS host:

```sh
cd firmware/tdisplay-s3
pio run                 # downloads LovyanGFX into .pio/
make -C snapshot        # writes PPM images to snapshot/build/
```
