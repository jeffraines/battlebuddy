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

**Cheap test:** print [size-cutouts.pdf](size-cutouts.pdf) (A4, 100% scale; regenerate with
`python3 tools/size_cutouts.py docs/size-cutouts.pdf`), cut out the outlines, and
play a game with them on the table before buying hardware.

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

## Display options

Two product variants are worth prototyping in parallel:

| | "Never charge it" (e-paper) | "Shiny" (AMOLED / LCD) |
|---|---|---|
| Look | Crisp like a printed card, B/W, no glow | Bright, colorful, animations |
| Battery | Weeks to months; maybe a coin cell or AAA, no charging | One play day (~8 h) with sleep/dimming, so it needs charging |
| Lighting | Reflective, great in bright shops, no light in the dark | Readable anywhere |
| Refresh | Partial refresh about 0.3 s, fine for counting | Instant |
| Input | Buttons / rotary encoder (small touch e-paper is rare) | Touch + a button or encoder |

Middle option: a **Sharp Memory LCD**. It's always on, uses microwatts, refreshes
fast with no ghosting, and has no backlight (reflective, like e-paper).

Notes:
- Color e-paper (B/W/red, B/W/red/yellow "(B)"/"(G)" panels, Spectra 6) takes
  10+ seconds to refresh, flashing, and can't partially refresh. That rules it
  out for a counter; when buying, pick the black-white variant.
- A raw e-paper panel needs a driver board (it generates the panel's high
  voltages). Buy the "module"/"HAT" version, not "raw display only".
- Touch film on e-paper lowers contrast. Small touch e-paper modules exist
  (Waveshare 2.13" Touch e-Paper HAT, SPI + I2C), but buttons fit e-paper better.
- AMOLED pixels are off when black, so a dark UI is both eye-catching and
  easier on the battery than a backlit LCD.
- On the e-paper variant, the enclosure and the art carry the looks: game-themed
  shells and crisp faction/icon artwork.

## Display test kit

**First prototype board: LilyGO T-Display S3 Touch** (ESP32-S3, 1.9" 170×320
capacitive touch). It has LiPo charging and a battery connector on board, plus
two side buttons that can serve as power/menu. Check the battery connector's
polarity before plugging in a LiPo; JST pinouts vary between sellers.

A 1.54" 240×240 IPS (already on hand) felt too small, so the test kit skips
anything that size or smaller. The 1.54" is still fine for developing firmware.

Shortlist after comparing actual-size cutouts (1.9" is the smallest that feels
right; non-square is preferred):

| Display | Example part | Approx. | Tests |
|---|---|---|---|
| 1.9" LCD touch, 170×320 | LilyGO T-Display S3 (touch) | $20–25 | First working prototype, personal size |
| 2.13" touch e-paper, 250×122 | Waveshare 2.13" Touch e-Paper HAT | $20–25 | The "never charge it" variant |
| 2.13" / 2.9" B/W e-paper module | WeAct Studio e-paper module, **black-white** version (driver board + pin header included) | ~$9–11 each | Cheapest e-paper prototype on the nRF52840 |
| 2.41" AMOLED touch, 600×450 | Waveshare ESP32-S3 AMOLED 2.41" (confirm listing) | $35–45 | The premium variant |
| 2.7" memory LCD, 400×240 | Adafruit Sharp Memory Display | $35–45 | Always-on low-power option (also for DigiPal) |
| 2.13" reflective mono TFT, 122×250 | AliExpress "2.13 Reflective TFT Panel" (Osptek), with adapter; ask seller for controller IC | ~$14 shipped | Cheap reflective/no-backlight option, alternative to the Sharp |
| 2.8" LCD touch, 320×240 | ESP32-2432S028R "Cheap Yellow Display" | $12–15 | Shared mode: one device between both players |

Skipped: 1.54" e-paper and 1.8" AMOLED (too close to the 1.54" IPS on hand).

Measure actual current draw on each board (for example with a Nordic PPK2) for
a scripted "8-hour game day" so the battery numbers are measured, not guessed.

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
- E-paper, AMOLED/LCD, or both as separate variants?
- Enclosure: 3D printed first; how thick can the battery be?
