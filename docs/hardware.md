# Hardware notes

## MCU: nRF52840 vs ESP32

Only Bluetooth LE is needed (setup + updates). Wi-Fi isn't required.

| | nRF52840 | ESP32-S3 / C6 |
|---|---|---|
| BLE + power | Best-in-class low power; mature BLE DFU (MCUboot + SMP) | Good enough; Wi-Fi can stay off |
| RAM for the display | 256 KB, fine for a small SPI LCD with LVGL partial buffers | S3: 512 KB + optional PSRAM, built for displays |
| Off-the-shelf touch boards | Few (PineTime, XIAO nRF52840 + round display, T-Echo e-paper) | Lots (Waveshare, LilyGO, "Cheap Yellow Display"), many with LiPo charging |
| Module cost | ~$4–8 | ~$2–4 |

The display and backlight use far more power than the MCU while the screen is
on, so the MCU's power advantage mostly shows up in sleep.

**Plan:** use **Zephyr + LVGL** so the choice can wait. Zephyr supports both
families, and moving between them is mostly a board/devicetree change. Keep the
game logic as portable C with a thin hardware layer.

- Now: develop the logic on the nRF52840 boards on hand, using buttons as input.
- Next: buy one or two cheap ESP32-S3 touch boards to test screen size and touch.
- Later: pick the production MCU based on what the prototypes show.

## Screen size

| Use | Suggested size | Why |
|---|---|---|
| Personal counter | ~1.7–2.0" | One big number, readable across the table (~15 mm digits) |
| Shared, between players | ~2.8–3.5" (320×240 / 480×320) | Two mirrored halves, each with real touch targets |

A 1.47" 172×320 screen can show one big number, but it's tight for touch
targets (aim for ≥ 9 mm) and too small for a shared two-player layout.

**Cheap test:** cut paper rectangles at 1.5", 2", 2.8", and 3.5" and play a
game with them on the table before buying hardware.

## Input

- Touch for menus and profile selection.
- Consider **physical +/- buttons or a rotary encoder** for changing the count.
  You can use them without looking, and stray touches from cards and sleeves
  can't change the number.
- A dedicated power/wake button either way.
- Touch lock or a "confirm" gesture to prevent accidental changes.

## Power

- USB-C charging plus a LiPo (~500 mAh as a starting point).
- Screen dims or sleeps between interactions, wakes on touch or button.
- Target: a full tournament day (8+ hours) on one charge.

## Alternative: e-paper

1.5–2.1" e-paper (e.g. LilyGO T-Echo, nRF52840) holds the number at zero
power, has no glare, and could last weeks on a charge. The trade-offs are slower
refresh and fewer touch options. Worth one prototype.

## Updates and configuration

- **Game profiles** are small data files (see `game-profiles.md`) pushed over
  BLE. Adding a game doesn't need new firmware.
- **Firmware** updates over BLE using MCUboot + SMP (works on nRF and
  ESP32 under Zephyr), with USB as a fallback.
- **Configurator:** a web page using Web Bluetooth, so there's no app to install.
  Web Bluetooth works in Chrome on desktop and Android, but not in Safari on iOS,
  so iPhone users would need a fallback.

## Open questions

- Personal device, shared device, or both (two screen sizes)?
- Touch only, or touch plus a physical encoder or buttons?
- LCD or e-paper?
- Enclosure: 3D printed first; how thick can the battery be?
