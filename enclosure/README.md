# Prototype case: LilyGO T-Display-S3 Touch + 803040 battery

![Case preview](preview.png)

A two-part 3D-printed case: a **shell** that holds the board face-down behind a
front window with the battery in its own bay underneath, and a **lid** that
screws on with 4 x M2 x 6 self-tapping screws. The battery swaps out by
removing the lid. Outer size about **67 x 35 x 24 mm**.

| File | What |
|---|---|
| `lilygo_case.scad` | Parametric OpenSCAD model; edit the values at the top |
| `shell.stl` | Print face-down (front on the bed), no supports |
| `lid.stl` | Print flat, countersinks on the bed |
| `fitcheck.stl` | Just the front frame and board walls; prints in minutes |

## Measure first

LilyGO lists the board as 62 x 26 x 10 mm but doesn't publish button or USB
positions. Before a full print, measure your board (or print `fitcheck.stl`)
and edit these in `lilygo_case.scad`:

- `board_l`, `board_w`, `board_t`: thickness including the touch glass and the
  parts on the back.
- `usb_from_front`: front glass to the centre of the USB-C port.
- `slots_left`, `slots_right`: where the BOOT / KEY / RST buttons are, in mm
  from the USB end. The defaults are long slots (4-26 mm) on both sides; shorten
  them once you know.

Then re-export:

```sh
openscad -D 'part="shell"' -o shell.stl lilygo_case.scad
openscad -D 'part="lid"'   -o lid.stl   lilygo_case.scad
```

## Assembly

1. Plug the battery into the board (check polarity: red to +).
2. Drop the board face-down into the shell, screen against the window.
3. Route the battery wire through the slot in the floor and lay the battery
   in the bay.
4. Screw the lid on. Stick four 8 mm rubber feet into the recesses.

If the board rattles, a strip of foam tape behind it takes up the slack.

## Notes

- PLA or PETG, 0.2 mm layers. The floor between board and battery bridges
  about 32 mm, which most printers handle.
- The battery must have a **JST 1.25 mm** (MX1.25) plug for the LilyGO. Listings
  often default to PH2.0, which won't fit.
