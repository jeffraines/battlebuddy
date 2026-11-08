# Prototype case: LilyGO T-Display-S3 Touch + 803040 battery

![Case preview](preview.png)

A 3D-printed case: a **shell** that holds the board face-down behind a front
window with the battery in its own bay underneath (a printed separator plate
keeps them apart, with a small notch for the wire), a **lid** that screws on
with 4 x M2 x 6 self-tapping screws (remove it to swap the battery), and
**button caps** that sit on the board's BOOT and KEY buttons and poke out of
the wall, so no membrane is needed. Reset is a small recessed pinhole so it can't be hit by
accident. Outer size about **67 x 35 x 24 mm**.

| File | What |
|---|---|
| `lilygo_case.scad` | Parametric OpenSCAD model; edit the values at the top |
| `shell.stl` | Print face-down (front on the bed), no supports |
| `lid.stl` | Print flat, countersinks on the bed |
| `buttons.stl` | The press plungers, laid out flat; no supports |
| `fitcheck.stl` | Just the front frame and board walls; prints in minutes |

## Measure first

LilyGO lists the board as 62 x 26 x 10 mm but doesn't publish button or USB
positions. Before a full print, measure your board (or print `fitcheck.stl`)
and edit these in `lilygo_case.scad`:

- `board_l`, `board_w`, `board_t`: thickness including the touch glass and the
  parts on the back.
- `usb_from_front`: front glass to the centre of the USB-C port.
- `buttons`: where each button is and whether it gets a cap (`"press"`) or a
  pinhole (`"pin"`). Sides are `"left"` / `"right"` (long edges, position in mm
  from the USB end) or `"end"` (the short edge opposite USB, position in mm
  from the centre line). **The defaults are guesses** (BOOT on the left and KEY
  on the right at 12 mm, reset as a pinhole on the left at 24 mm). The fit check has long slots on both sides instead,
  so you can see where the buttons really are.
- `button_from_front`: front glass to the centre of the buttons.
- `button_stick_out`: how far the buttons stick out past the board edge. This
  sets how far the plungers reach; if they don't click, lower it a little.

Then re-export:

```sh
openscad -D 'part="shell"' -o shell.stl lilygo_case.scad
openscad -D 'part="lid"'     -o lid.stl     lilygo_case.scad
openscad -D 'part="buttons"' -o buttons.stl lilygo_case.scad
```

## Assembly

1. Plug the battery into the board (check polarity: red to +).
2. Push the plungers into their holes from the inside, caps out. Their inner
   blocks are bigger than the holes, so the board holds them in place.
3. Drop the board face-down into the shell, screen against the window.
4. Route the battery wire through the slot in the floor and lay the battery
   in the bay.
5. Screw the lid on. Stick four 8 mm rubber feet into the recesses.

If the board rattles, a strip of foam tape behind it takes up the slack.

## Notes

- The USB-C opening is 13 x 8 mm with the wall thinned around it so the plug's
  moulded housing can get close enough to seat. If a chunky cable still won't
  click in, raise `usb_relief` or use a slimmer cable.

- PLA or PETG, 0.2 mm layers. The floor between board and battery bridges
  about 32 mm, which most printers handle.
- The battery must have a **JST 1.25 mm** (MX1.25) plug for the LilyGO. Listings
  often default to PH2.0, which won't fit.
