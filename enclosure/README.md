# Prototype case: LilyGO T-Display-S3 Touch + 803040 battery

![Case preview](preview.png)

A 3D-printed case: a **shell** that holds the board face-down behind a front
window with the battery in its own bay underneath (a printed separator plate
keeps them apart, with a small notch for the wire), a **lid** that screws on
with 4 x M2 x 6 self-tapping screws (remove it to swap the battery), and
**button caps**: the T-Display-S3's BOOT and KEY buttons are on its face,
either side of the USB port, so the front frame covers that bare end of the
board and two round caps poke up through it and sit on the buttons (no
membrane needed). Outer size about **67 x 37 x 25 mm**.

| File | What |
|---|---|
| `lilygo_case.scad` | Parametric OpenSCAD model; edit the values at the top |
| `shell.stl` | Print face-down (front on the bed), no supports |
| `lid.stl` | Print flat, countersinks on the bed |
| `buttons.stl` | The press plungers, laid out flat; no supports |
| `fitcheck.stl` | Just the front frame and board walls; prints in minutes |

## Measure first

The Touch version is listed as 62 x 32 x 11 mm, but button and USB positions
aren't published. Before a full print, measure your board (or print
`fitcheck.stl`) and edit these in `lilygo_case.scad`:

- `board_l`, `board_w`, `board_t`: thickness including the touch glass and the
  parts on the back.
- `tail_l`: how far the bare board sticks out past the glass at the USB end.
- `active_from_far`, `key_from_glass`: where the display and the circle touch
  key sit on the glass. The front frame covers the glass's black border and
  shows only the display, with a small round hole for the key
  (`window_style = "glass"` shows the whole glass instead).
- `usb_from_front`: front of the glass to the centre of the USB-C port.
- `glass_drop`: from the front of the glass down to the board's top surface.
- `switch_h`: how tall the face buttons stand above the board.
- `buttons`: where each button is. The defaults put BOOT and KEY on the face,
  3.5 mm from the USB end and 10 mm either side of centre. **These are
  estimates from product photos.** Edge buttons (`"left"`, `"right"`, `"end"`)
  and recessed pinholes (`"pin"`) are also supported.
- If the caps don't click, lower `switch_h` a little so they reach further.

Then re-export:

```sh
openscad -D 'part="shell"' -o shell.stl lilygo_case.scad
openscad -D 'part="lid"'     -o lid.stl     lilygo_case.scad
openscad -D 'part="buttons"' -o buttons.stl lilygo_case.scad
```

## Assembly

1. Plug the battery into the board (check polarity: red to +).
2. Drop the two button caps into their holes from the inside, caps out.
   Their inner blocks are bigger than the holes, so the board holds them in.
3. Drop the board face-down into the shell, screen against the window.
4. Route the battery wire through the slot in the floor and lay the battery
   in the bay.
5. Screw the lid on. Stick four 8 mm rubber feet into the recesses.

Thin crush ribs on the inside walls squash as the board goes in so it sits
snug (`rib`; shave them with a knife if it's too tight). If it still rattles,
a strip of foam tape behind it takes up the slack.

## Notes

- The USB-C opening is 13 x 8 mm with the wall thinned around it so the plug's
  moulded housing can get close enough to seat. If a chunky cable still won't
  click in, raise `usb_relief` or use a slimmer cable.

- PLA or PETG, 0.2 mm layers. The floor between board and battery bridges
  about 32 mm, which most printers handle.
- The battery must have a **JST 1.25 mm** (MX1.25) plug for the LilyGO. Listings
  often default to PH2.0, which won't fit.
