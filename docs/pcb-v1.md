# PCB v1 requirements

A running list of what the first custom board needs. **Must** = v1 won't ship
without it, **Should** = planned, **Maybe** = leave a footprint or decide later.
Part numbers are candidates to evaluate, not final picks.

## Battery life target

- **Minimum: 8 hours with the screen on the whole time** (a full event day).
  The screen can't rely on dimming; it must stay readable throughout.
- **Goal: a weekend (two ~8 h days, ~16 h) without charging.**

Rough budget (to be measured on the prototype):

| Setup | Est. draw | 8 h needs | 16 h needs |
|---|---|---|---|
| Backlit LCD, full brightness, MCU as today | ~100 mA | ~1000 mAh | ~2000 mAh |
| Backlit LCD, moderate brightness, MCU slowed + light sleep | ~40 mA | ~400 mAh | ~800 mAh |
| Reflective / memory LCD (no backlight) | ~10 mA or less | tiny | ~200 mAh |

Add ~25% for battery aging (capacity falls to ~80% after a few hundred cycles).
So with a backlit screen, the weekend goal needs both firmware power work and a
~1000-1500 mAh cell; a reflective screen meets it easily. This is a strong
reason to evaluate the reflective panel from the test kit.

## Power

| | Requirement | Notes / candidates |
|---|---|---|
| Must | **Fuel gauge** (accurate battery %) | MAX17048 (no sense resistor, I2C, tiny) or BQ27441 (coulomb counter, needs a sense resistor; more accurate under changing loads). The T-Display-S3 only has a voltage divider, which is why its % is rough |
| Must | **User-replaceable battery** | See design principles in `roadmap.md`. Leaning: phone-style flat pack (BL-5C type, 3.7 V ~1000 mAh) on spring contacts, or a JST plug behind a battery door. Keyed / polarity-protected |
| Must | Li-ion charger with power path | Runs from USB while charging and doesn't confuse "charging" with "full". Low quiescent current |
| Must | Low-quiescent regulator | Standby life depends on it as much as on the MCU. Buck-boost (e.g. TPS63802 class) or a low-Iq LDO |
| Must | Reverse-polarity and battery protection | Protection on the board if the pack doesn't have its own |
| Must | USB-C for charging and programming | ESD protection; side-facing and recessed for spill tolerance |
| Should | **Wireless charging receiver** | Qi receiver feeding the same charger, e.g. BQ51050B (Qi receiver with built-in Li-ion charger) and a ~30-40 mm receiver coil with a ferrite sheet on the back. Keep the coil area free of copper pour and away from the radio antenna. Enables the drop-on-a-tray charging idea in `roadmap.md` |
| Maybe | Pogo-pin charging pads | Cheap fallback for the tray if wireless costs too much; footprint only |

## Core

| | Requirement | Notes / candidates |
|---|---|---|
| Must | MCU as a pre-certified module | Makes radio certification (KC / FCC / CE) much easier. Open: ESP32-S3 module (Wi-Fi + BLE, more headroom) vs nRF52840 module (best battery). See `roadmap.md` |
| Must | Display + capacitive touch on FPC connectors | Size from the display test kit (~2.0-2.8"). Connectors, not solder, so the screen is replaceable |
| Must | Physical buttons | At least wake/menu and undo (BOOT/KEY roles today). Sealed caps or a membrane |
| Must | Unique device ID | From the chip; needed for the tournament kit |
| Should | Accelerometer | e.g. LIS3DH: face-down to sleep, wake on pick-up, auto-rotate |
| Maybe | I2C expansion header (Qwiic style) | Handy for development and add-ons |

## Enclosure shape

- **Flat, not wedged.** In shared and facing modes the device lies between the
  players and must read from both sides, so it can't tilt toward one person.
- The battery sits evenly under the board. To keep the extra thickness
  (~16-18 mm with a ~1400 mAh cell) from looking bulky: tapered or rounded
  sides so the top face looks thinner (a "river stone" profile), and soft feet
  so it doesn't slide.
- Needs a wide-viewing-angle (IPS) display, since one player always reads it
  off-axis.

## Build and repair

| | Requirement | Notes |
|---|---|---|
| Must | Test points and programming access | USB plus a debug header or pads |
| Must | Mounting holes for screws | At least 3; the LilyGO's single hole was a limitation |
| Should | Conformal coating friendly | Keep connectors and the touch flex clear |
| Should | Room for a display gasket | Spill tolerance without an official IP rating |

## To decide before layout

- MCU family (measured battery life on the prototypes).
- Display size and type (test kit).
- Battery format (flat pack on contacts vs plug).
- Fuel gauge: MAX17048 vs BQ27441.
- Wireless charging: per-slot Qi vs one resonant tray coil vs pogo pins.
