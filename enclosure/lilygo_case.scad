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
// Board size is from LilyGO's listing (62 x 26 x 10 mm); button and USB
// positions are NOT published, so every opening is a parameter.
// MEASURE YOUR BOARD and edit the values marked "measure" before printing,
// or print the fit check first (part = "fitcheck").
//
// Export:  openscad -D 'part="shell"'   -o shell.stl   lilygo_case.scad
//          openscad -D 'part="lid"'     -o lid.stl     lilygo_case.scad
//          openscad -D 'part="buttons"' -o buttons.stl lilygo_case.scad

part = "assembly"; // "shell", "lid", "buttons", "fitcheck", "assembly" (preview only)

/* [Board: measure] */
board_l = 62;    // length, USB end to far end
board_w = 26;    // width
board_t = 10;    // thickness incl. display, touch glass and parts on the back
usb_from_front = 5;   // front glass to the centre of the USB-C port
lip = 1.2;            // how far the front frame overlaps the board edge

/* [Side buttons: measure] */
// One entry per button on the board's long edges:
//   [side, centre in mm from the USB end of the board, kind]
//   side: "left" / "right" (looking at the screen, USB end at the bottom)
//   kind: "press" = a printed plunger you push; "pin" = a small recessed hole
//         (for reset, so it can't be hit by accident)
// THESE POSITIONS ARE GUESSES. Measure your board (or print the fit check,
// which has long slots instead) and correct them.
buttons = [["left", 12, "press"],   // BOOT (GPIO0): menu
           ["left", 24, "press"],   // KEY (GPIO14): undo
           ["right", 12, "pin"]];   // RST
button_from_front = 5;   // front glass to the centre of the buttons (measure)
button_stick_out = 0.5;  // how far the buttons stick out past the board edge (measure)
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
wall = 2.0;
front = 1.6;      // front frame thickness
shelf = 1.4;      // floor between board and battery
lid_t = 1.8;
clr = 0.5;        // clearance around board and battery
corner_r = 4;
usb_w = 11;       // USB-C opening (plug overmould needs room)
usb_h = 7;
screw_pilot = 1.7;  // M2 self-tapping
screw_clear = 2.4;
boss_d = 5.5;

$fn = 48;

// Interior
in_l = max(board_l, batt_l) + 2 * clr;
in_w = max(board_w, batt_w) + 2 * clr;
bay_h = batt_t + 0.6;
board_z = bay_h + shelf;              // back of the board pocket
front_z = board_z + board_t + clr;    // inner face of the front frame
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;
out_h = front_z + front;

board_x = wall + clr;                     // board sits against the USB end
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
gap_side = (in_w - board_w) / 2;                 // wall to board edge
block_t = gap_side - button_stick_out - 0.2;     // plunger's inner block (presses the button)
block_l = cap_l + 3;
block_h = cap_h + 3;

function side_y(side) = side == "left" ? 0 : out_w;

// Hole through the wall (and the pocket in the rail behind it) for one button.
module button_hole(b) {
    x = board_x + b[1];
    if (b[2] == "pin") {
        y0 = b[0] == "left" ? -1 : out_w - wall - gap_side - 1;
        translate([x, b[0] == "left" ? -1 : out_w + 1, btn_z])
            rotate([b[0] == "left" ? -90 : 90, 0, 0]) cylinder(d = pin_d, h = wall + gap_side + 2);
    } else {
        // Cap hole through the wall.
        translate([x - cap_l / 2 - plunger_clr, b[0] == "left" ? -1 : out_w - wall - 1, btn_z - cap_h / 2 - plunger_clr])
            cube([cap_l + 2 * plunger_clr, wall + 2, cap_h + 2 * plunger_clr]);
        // Room for the plunger's inner block.
        translate([x - block_l / 2 - 0.3, b[0] == "left" ? wall - 0.01 : out_w - wall - gap_side, btn_z - block_h / 2 - 0.3])
            cube([block_l + 0.6, gap_side + 0.01, block_h + 0.6]);
    }
}

// One plunger, oriented as it sits in a left-side hole (pressing toward +y).
module plunger() {
    // Inner block, larger than the hole so the plunger can't fall out.
    translate([-block_l / 2, wall, -block_h / 2]) cube([block_l, block_t, block_h]);
    // Stem and cap through the wall, standing proud of the outside.
    translate([-cap_l / 2, -cap_proud, -cap_h / 2]) cube([cap_l, wall + cap_proud + 0.01, cap_h]);
}

module shell(with_slots = false) {
    difference() {
        rounded_box(out_l, out_w, out_h, corner_r);

        // Battery bay (open at the back, closed by the lid).
        translate([wall, wall, -1]) cube([in_l, in_w, bay_h + 1]);

        // Board pocket.
        translate([wall, wall, board_z]) cube([in_l, in_w, front_z - board_z]);

        // Front window: the board's outline minus the lip.
        translate([board_x + lip, board_y + lip, front_z - 1])
            cube([board_l - 2 * lip, board_w - 2 * lip, front + 2]);

        // Wire pass-through from battery bay to the board's battery connector.
        translate([wall + 6, out_w / 2 - 5, bay_h - 1]) cube([10, 10, shelf + 2]);

        // USB-C at the board end.
        usb_z = front_z - usb_from_front;
        translate([-1, out_w / 2 - usb_w / 2, usb_z - usb_h / 2]) cube([wall + 2, usb_w, usb_h]);

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
                translate([wall - 1, out_w / 2 - usb_w / 2, 0]) cube([3, usb_w, out_h]);
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

// A thin frame of just the front and board walls: prints in a few minutes and
// shows whether the board, USB port and buttons line up before a full print.
module fitcheck() {
    intersection() {
        shell(with_slots = true);
        translate([-1, -1, board_z + board_t * 0.35]) cube([out_l + 2, out_w + 2, out_h]);
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
        translate([i * (block_l + 4) + block_l / 2, block_h / 2, 0])
            rotate([-90, 0, 0]) translate([0, -wall - block_t, 0]) plunger();
} else if (part == "fitcheck") {
    translate([0, out_w, out_h]) rotate([180, 0, 0]) fitcheck();
} else {
    color("SteelBlue", 0.85) shell();
    board_dummy();
    battery_dummy();
    for (b = buttons)
        if (b[2] == "press")
            color("Orange")
                translate([board_x + b[1], side_y(b[0]), btn_z])
                    mirror([0, b[0] == "left" ? 0 : 1, 0]) plunger();
    color("SlateGray") translate([0, 0, -lid_t - 6]) lid();
}

echo(str("Outer size: ", out_l, " x ", out_w, " x ", out_h + lid_t, " mm"));
