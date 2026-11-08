// BattleBuddy prototype case: LilyGO T-Display-S3 Touch + 803040 LiPo
// (8 x 30 x 40 mm, 1000 mAh).
//
// Printed parts plus 4 x M2 x 6 self-tapping screws:
//   shell   - holds the board face-down behind a front window, with the
//             battery in its own bay underneath (print face-down, no supports)
//   lid     - screws onto the back; remove it to swap the battery
//   buttons - press plungers that sit in the side walls and push the board's
//             side buttons (print as laid out, no supports)
//
// Board size is from the Touch version's listing (62 x 32 x 11 mm). The
// screen glass covers most of the front; at the USB end the bare board sticks
// out with the USB-C port in the middle and two face buttons (BOOT, KEY), one
// each side. Exact positions aren't published, so every opening is a parameter.
// MEASURE YOUR BOARD and edit the values marked "measure" before printing,
// or print the fit check first (part = "fitcheck").
//
// Export:  openscad -D 'part="shell"'   -o shell.stl   lilygo_case.scad
//          openscad -D 'part="lid"'     -o lid.stl     lilygo_case.scad
//          openscad -D 'part="buttons"' -o buttons.stl lilygo_case.scad

part = "assembly";
clr_default = 0.5; // "shell", "lid", "buttons", "fitcheck", "assembly" (preview only)

/* [Board: measure] */
board_l = 62;    // length, USB end to far end
board_w = 32;    // width
board_t = 11;    // thickness incl. display, touch glass and parts on the back
tail_l = 8;      // bare board at the USB end, beyond the glass
glass_drop = 4;  // front of the glass down to the board's top surface
switch_h = 2.2;  // face buttons: height above the board's top surface
usb_from_front = 6.2;  // front glass to the USB-C centre (the first print showed it sits lower)
lip = 1.2;            // how far the front frame overlaps the glass edge (window_style "glass")

/* [Front window: measure] */
// "screen": the frame covers the glass's black border and shows the display
//           plus the strip below it with the circle touch key, as one window.
// "border": like "screen", but most of the glass's black border shows too, so
//           the plastic frame is thin (best with black filament: glass and
//           frame blend). The board itself is black, so a slight overshoot hides.
// "glass":  the whole glass shows.
window_style = "border";
active_w = 22.7;        // visible display area (1.9" 170x320 panel)
active_l = 42.7;
active_from_far = 6.3;  // far edge of the glass to the start of the display (measure)
screen_margin = 0.4;    // "screen": window this much bigger than the display all round
border_side = 3.0;      // "border": window this much wider than the display on each long side
border_far = 2.5;       //           and this much past it at the far end
bevel = 1.2;            // window edges slope outward by this much (room for a fingertip)
key_from_glass = 3.9;   // USB-side edge of the glass to the circle key's centre (measure)
key_room = 3.5;         // window reaches this far past the key's centre toward USB

/* [Fit] */
board_clr = 0.25;       // gap around the board in its pocket (sides and ends)
rib_squeeze = 0.08;     // crush ribs reach this far past the board edge (r05 0.3 bent the screen, r06 -0.15 rattled)
rib = board_clr + rib_squeeze;           // side ribs
end_rib = 2 * board_clr + rib_squeeze;   // far-end ribs: also push the board against the USB end

/* [Buttons: measure] */
// One entry per button:
//   [side, position, kind, across]
//   side: "front" = on the board's face (the T-Display-S3's BOOT and KEY sit
//         either side of the USB port); position = centre in mm from the USB
//         end; across = offset in mm from the centre line (+ = right, looking
//         at the screen with the USB end at the bottom).
//         "left" / "right" = the long edges; position = from the USB end.
//         "end" = the short edge opposite USB; position = offset from centre.
//   kind: "press" = a printed cap that sits on the board's button and pokes out
//         of the case; "pin" = a small recessed hole (so it can't be hit by accident)
// THESE POSITIONS ARE ESTIMATES from product photos. Measure and correct them.
buttons = [["front", 3.5, "press", -10.75],   // BOOT (GPIO0): menu  (r05 +-10 too far in, r06 +-11.5 too far out)
           ["front", 3.5, "press", 10.75]];   // KEY (GPIO14): undo
button_from_front = 5;   // side / end buttons: front glass to their centre (measure)
button_stick_out = 0.5;  // side / end buttons: how far they stick out past the board edge
face_cap_d = 5;          // round cap on face buttons
face_block = [6.5, 8];   // face button's inner block (along, across)
cap_l = 7;               // plunger cap, along the case
cap_h = 4;               // plunger cap height
cap_proud = 1.2;         // how far the cap stands out of the wall
plunger_clr = 0.25;      // gap around the cap in its hole
pin_d = 2.2;             // reset pinhole

// Fit check: long slots on both sides instead of buttons, to find positions.
slots_left  = [[4, 30]];
slots_right = [[4, 30]];
slot_h = 6;

/* [Battery] */
batt_l = 40;
batt_w = 30;
batt_t = 8;

/* [Case] */
wall = 1.6;
front = 1.2;      // front frame thickness
edge_ch = 1.0;    // chamfer round the front edge so the case looks thinner
shelf = 1.2;      // separator plate between board and battery
wire_notch_x = 10;   // where the battery wire passes the separator, mm from the USB end
wire_notch = [6, 4]; // notch size (along the case, across)
lid_t = 1.4;
clr = clr_default; // clearance around board and battery
corner_r = 4;
usb_w = 9.6;      // USB-C opening: just the plug's metal shell (8.4 x 2.6) plus room for error
usb_h = 4.2;
usb_web = 0.8;    // wall left round the port (thinned from inside) so the plug housing gets close
screw_pilot = 1.7;  // M2 self-tapping
screw_clear = 2.4;
boss_d = 5.5;

$fn = 48;

// Interior
has_end_buttons = len([for (b = buttons) if (b[0] == "end") b]) > 0;
end_room = has_end_buttons ? (max(board_w, batt_w) - board_w) / 2 + clr : 0;
in_l = max(board_l + end_room + 2 * board_clr, batt_l + 2 * clr);
in_w = max(board_w + 2 * board_clr, batt_w + 2 * clr);
bay_h = batt_t + 0.6;
board_z = bay_h + shelf;              // back of the board pocket
front_z = board_z + board_t + clr;    // inner face of the front frame
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;
out_h = front_z + front;
usb_z = front_z - usb_from_front;          // USB-C centre height

// Rounded-rectangle USB slot outline (in the YZ plane after rotation), grown by g.
module usb_slot(g) {
    r = usb_h / 2 + g;
    hull() for (y = [-usb_w / 2 + usb_h / 2, usb_w / 2 - usb_h / 2]) translate([0, y]) circle(r = r, $fn = 48);
}

board_x = wall + board_clr;                     // board sits against the USB end
board_y = wall + (in_w - board_w) / 2;
batt_x = wall + (in_l - batt_l) / 2;      // battery centred, clear of the screw bosses
batt_y = wall + (in_w - batt_w) / 2;

boss_xy = [[wall + boss_d / 2 - 0.5, wall + boss_d / 2 - 0.5],
           [out_l - wall - boss_d / 2 + 0.5, wall + boss_d / 2 - 0.5],
           [wall + boss_d / 2 - 0.5, out_w - wall - boss_d / 2 + 0.5],
           [out_l - wall - boss_d / 2 + 0.5, out_w - wall - boss_d / 2 + 0.5]];

module rounded_box(l, w, h, r) {
    hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}

btn_z = front_z - button_from_front;
gap_side = (in_w - board_w) / 2;                 // wall to board edge, long sides
block_l = cap_l + 3;
block_h = cap_h + 3;

// Gap between the inside of the wall and the button on a button's side.
function button_gap(b) = b[0] == "front" ? clr + glass_drop - switch_h
                       : b[0] == "end" ? out_l - wall - (board_x + board_l) - button_stick_out
                       : gap_side - button_stick_out;
// Wall thickness the cap passes through.
function wall_of(b) = b[0] == "front" ? front : wall;
// Thickness of a cap's inner block (the part that sits on the button).
function block_thick(b) = button_gap(b) - 0.2;

// Places children in a button's frame: origin on the outside of the case at
// the button's centre, +y pointing into the case.
module at_button(b) {
    if (b[0] == "front")
        translate([board_x + b[1], board_y + board_w / 2 + b[3], out_h]) rotate([-90, 0, 0]) children();
    else if (b[0] == "left")
        translate([board_x + b[1], 0, btn_z]) children();
    else if (b[0] == "right")
        translate([board_x + b[1], out_w, btn_z]) mirror([0, 1, 0]) children();
    else
        translate([out_l, board_y + board_w / 2 + b[1], btn_z]) rotate([0, 0, 90]) children();
}

// Hole through the wall (and the pocket behind it) for one button.
module button_hole(b) {
    w = wall_of(b);
    at_button(b) {
        if (b[2] == "pin") {
            translate([0, -1, 0]) rotate([-90, 0, 0]) cylinder(d = pin_d, h = w + button_gap(b) + 2);
        } else if (b[0] == "front") {
            translate([0, -1, 0]) rotate([-90, 0, 0]) cylinder(d = face_cap_d + 2 * plunger_clr, h = w + 2);
            translate([-face_block[0] / 2 - 0.3, w - 0.01, -face_block[1] / 2 - 0.3])
                cube([face_block[0] + 0.6, button_gap(b) + 0.01, face_block[1] + 0.6]);
        } else {
            translate([-cap_l / 2 - plunger_clr, -1, -cap_h / 2 - plunger_clr])
                cube([cap_l + 2 * plunger_clr, w + 2, cap_h + 2 * plunger_clr]);
            translate([-block_l / 2 - 0.3, w - 0.01, -block_h / 2 - 0.3])
                cube([block_l + 0.6, button_gap(b) + 0.01, block_h + 0.6]);
        }
    }
}

// A press button: a cap that pokes out of the case, on an inner block that is
// bigger than the hole (so it can't fall out) and sits on the board's button.
module plunger(b) {
    t = block_thick(b);
    w = wall_of(b);
    if (b[0] == "front") {
        translate([-face_block[0] / 2, w, -face_block[1] / 2]) cube([face_block[0], t, face_block[1]]);
        translate([0, -cap_proud, 0]) rotate([-90, 0, 0]) cylinder(d = face_cap_d, h = w + cap_proud + 0.01);
    } else {
        translate([-block_l / 2, w, -block_h / 2]) cube([block_l, t, block_h]);
        translate([-cap_l / 2, -cap_proud, -cap_h / 2]) cube([cap_l, w + cap_proud + 0.01, cap_h]);
    }
}

module shell(with_slots = false) {
    difference() {
        hull() {
            rounded_box(out_l, out_w, out_h - edge_ch, corner_r);
            translate([edge_ch, edge_ch, 0]) rounded_box(out_l - 2 * edge_ch, out_w - 2 * edge_ch, out_h, corner_r - edge_ch);
        }

        // Battery bay (open at the back, closed by the lid).
        translate([wall, wall, -1]) cube([in_l, in_w, bay_h + 1]);

        // Board pocket.
        translate([wall, wall, board_z]) cube([in_l, in_w, front_z - board_z]);

        // Front window.
        if (window_style == "screen" || window_style == "border") {
            // The display plus the circle key below it, as one bevelled window.
            wx = board_x + tail_l + key_from_glass - key_room;
            side = window_style == "border" ? border_side : screen_margin;
            far = window_style == "border" ? border_far : screen_margin;
            wy = out_w / 2 - active_w / 2 - side;
            wl = board_x + board_l - active_from_far + far - wx;
            ww = active_w + 2 * side;
            hull() {
                translate([wx, wy, front_z - 0.01]) cube([wl, ww, 0.01]);
                translate([wx - bevel, wy - bevel, out_h]) cube([wl + 2 * bevel, ww + 2 * bevel, 0.01]);
            }
            translate([wx, wy, front_z - 1]) cube([wl, ww, 1.02]);
        } else {
            // The whole glass (the frame covers the bare board at the USB end).
            translate([board_x + tail_l + lip, board_y + lip, front_z - 1])
                cube([board_l - tail_l - 2 * lip, board_w - 2 * lip, front + 2]);
        }

        // Small notch in the separator for the battery wire.
        translate([board_x + wire_notch_x - wire_notch[0] / 2, out_w / 2 - wire_notch[1] / 2, bay_h - 1])
            cube([wire_notch[0], wire_notch[1], shelf + 2]);

        // USB-C at the board end.
        // Small rounded slot for the plug's metal shell only.
        translate([-1, out_w / 2, usb_z]) rotate([0, 90, 0]) linear_extrude(wall + 2) usb_slot(0);
        // Wall thinned from the inside round the port so the plug housing gets close enough to seat.
        translate([usb_web, out_w / 2, usb_z]) rotate([0, 90, 0]) linear_extrude(wall) usb_slot(2.5);

        if (with_slots) {
            // Fit check: long slots in both long walls.
            for (s = slots_left)
                translate([board_x + s[0], -1, btn_z - slot_h / 2]) cube([s[1] - s[0], wall + 2, slot_h]);
            for (s = slots_right)
                translate([board_x + s[0], out_w - wall - 1, btn_z - slot_h / 2]) cube([s[1] - s[0], wall + 2, slot_h]);
        } else {
            for (b = buttons) button_hole(b);
        }
    }

    // Side rails that centre the narrower board in the pocket.
    rail = (in_w - board_w) / 2 - 0.2;
    if (rail > 0.4)
        for (y = [wall, out_w - wall - rail])
            difference() {
                translate([wall, y, board_z]) cube([in_l, rail, front_z - board_z]);
                if (with_slots) {
                    for (s = concat(slots_left, slots_right))
                        translate([board_x + s[0], y - 1, btn_z - slot_h / 2]) cube([s[1] - s[0], rail + 2, slot_h]);
                } else {
                    for (b = buttons) button_hole(b);
                }
                translate([wall - 1, out_w / 2, usb_z]) rotate([0, 90, 0]) linear_extrude(3) usb_slot(0);
            }

    // With buttons on the far end, fill the extra room there so the board can't slide.
    if (has_end_buttons)
        difference() {
            translate([board_x + board_l + 0.2, wall, board_z]) cube([out_l - wall - board_x - board_l - 0.2, in_w, front_z - board_z]);
            for (b = buttons) if (b[0] == "end") button_hole(b);
        }

    // Crush ribs: thin ridges on the inside walls of the board pocket that
    // squash as the board goes in, so it sits snug instead of rattling.
    if (rib > 0) {
        for (x = [board_x + 15, board_x + 31, board_x + 47]) {
            translate([x - 0.5, wall - 0.01, board_z]) cube([1, rib, front_z - board_z]);
            translate([x - 0.5, out_w - wall - rib + 0.01, board_z]) cube([1, rib, front_z - board_z]);
        }
        for (y = [out_w / 2 - 9, out_w / 2 + 9])
            translate([out_l - wall - end_rib + 0.01, y - 0.5, board_z]) cube([end_rib, 1, front_z - board_z]);
    }

    // Screw bosses in the battery bay corners (the battery sits between them).
    for (p = boss_xy)
        translate([p[0], p[1], 0])
            difference() {
                cylinder(d = boss_d, h = bay_h);
                translate([0, 0, -1]) cylinder(d = screw_pilot, h = bay_h);
            }
}

module lid() {
    difference() {
        rounded_box(out_l, out_w, lid_t, corner_r);
        for (p = boss_xy)
            translate([p[0], p[1], -1]) {
                cylinder(d = screw_clear, h = lid_t + 2);
                cylinder(d1 = 4.4, d2 = screw_clear, h = 1.2 + 1); // countersink, outside face
            }
        // Recesses for four 8 mm stick-on rubber feet.
        for (x = [10, out_l - 10], y = [9, out_w - 9])
            translate([x, y, -1]) cylinder(d = 8.6, h = 1.6);
    }
}

// Quick fit check: the front frame with short walls (enough to check the
// outline, window, key hole and button holes), plus the USB end at full
// depth so the port can be checked too. Prints in a fraction of the time.
fit_walls = 4;     // wall height below the front frame
fit_usb_len = 6;   // length of the full-depth piece at the USB end

module fitcheck() {
    intersection() {
        shell();
        union() {
            translate([-1, -1, front_z - fit_walls]) cube([out_l + 2, out_w + 2, out_h]);
            translate([-1, -1, board_z]) cube([fit_usb_len + 1, out_w + 2, out_h]);
        }
    }
}

module board_dummy() {
    color("DimGray") translate([board_x, board_y, board_z + clr]) cube([board_l, board_w, board_t]);
}

module battery_dummy() {
    color("Silver") translate([batt_x, batt_y, 0.3]) cube([batt_l, batt_w, batt_t]);
}

if (part == "shell") {
    // Printed face-down.
    translate([0, out_w, out_h]) rotate([180, 0, 0]) shell();
} else if (part == "lid") {
    lid();
} else if (part == "buttons") {
    // Laid out flat: inner block on the bed, cap pointing up.
    presses = [for (b = buttons) if (b[2] == "press") b];
    for (i = [0 : len(presses) - 1])
        translate([i * (block_l + 4) + block_l / 2, block_l / 2, 0])
            rotate([-90, 0, 0]) translate([0, -wall_of(presses[i]) - block_thick(presses[i]), 0]) plunger(presses[i]);
} else if (part == "fitcheck") {
    translate([0, out_w, out_h]) rotate([180, 0, 0]) fitcheck();
} else {
    color("SteelBlue", 0.85) shell();
    board_dummy();
    battery_dummy();
    for (b = buttons)
        if (b[2] == "press")
            color("Orange") at_button(b) plunger(b);
    color("SlateGray") translate([0, 0, -lid_t - 6]) lid();
}

echo(str("Outer size: ", out_l, " x ", out_w, " x ", out_h + lid_t, " mm"));
