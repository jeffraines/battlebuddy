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

**Phase 1 order (final, ~₩63,000).** Covers touch, reflective, e-paper, an odd
bar shape, and a 4:3 2.8" across 1.9"–2.9".

| Display | Price (KRW) | Covers | MCU |
|---|---|---|---|
| LilyGO T-Display S3 Touch, **full board** (not the bare-screen option) + 3.7 V LiPo (JST 1.25 mm) | ~₩20–30k | 1.9" capacitive touch LCD, battery; handheld prototype | ESP32-S3 (on board) |
| Osptek 2.13" reflective TFT (ST7305), with adapter | ~₩13,900 shipped | Reflective; DigiPal | Both |
| WeAct 2.9" e-paper, Black-White (WeAct Studio store) | ₩9,030 + ₩3,819 shipping | E-paper, 2.9" strip | Both |
| Estardyn 2.79" bar TFT, 142×428 (NV3007, SPI) | ₩6,500 | Long bar form factor, backlit color | Both |
| 0.91" OLED 128×32 (SSD1306, I2C) | ₩2,160 | Debug/status display; small-OLED look | Both (built-in Zephyr driver) |
| Estardyn 2.8" TFT 240×320 (ST7789V, SPI), "2.8 Inch" variant | ₩8,500 | Largest number (43 mm short side), shared-mode layout; well-supported driver | Both |

The NV3007 controller is uncommon, so expect to port its init sequence. The
2.8" board can also run in 8080 parallel mode; check its mode jumpers are set
for 4-wire SPI.

Not in this round: Good Display touch e-paper (2.13" ~₩30k+ with its driver
board; revisit if e-paper wins), Osptek 4.2" (too big), AMOLED and Sharp
(cost), Cheap Yellow Display (resistive).

Shortlist after comparing actual-size cutouts (1.9" is the smallest that feels
right; non-square is preferred):

| Display | Example part | Approx. | Tests |
|---|---|---|---|
| 1.9" LCD touch, 170×320 | LilyGO T-Display S3 (touch) | $20–25 | First working prototype, personal size |
| 2.13" touch e-paper, 250×122 | Waveshare 2.13" Touch e-Paper HAT | $20–25 | The "never charge it" variant |
| 2.13" / 2.9" B/W e-paper module | WeAct Studio e-paper module, **black-white** version (driver board + pin header included) | ~$9–11 each | Cheapest e-paper prototype on the nRF52840 |
| 2.41" AMOLED touch, 600×450 | Waveshare ESP32-S3 AMOLED 2.41" (confirm listing) | $35–45 | The premium variant |
| 2.7" memory LCD, 400×240 | Adafruit Sharp Memory Display | $35–45 | Always-on low-power option (also for DigiPal) |
| 2.13" reflective mono TFT, 122×250 | AliExpress "2.13 Reflective TFT Panel" (Osptek), with adapter; ST7305 controller | ~$14 shipped | Cheap reflective/no-backlight option, alternative to the Sharp |
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

## Driver support per platform

The nRF52840 runs Zephyr (nRF Connect SDK), so Arduino libraries (TFT_eSPI,
LovyanGFX, Arduino_GFX, GxEPD2) don't apply there; what matters is Zephyr's
display drivers. LVGL sits on top of either.

| Display | nRF52840 (Zephyr) | LilyGO ESP32-S3 (Arduino / ESP-IDF) |
|---|---|---|
| Estardyn 2.8" ST7789V | Built-in driver (`sitronix,st7789v`) | TFT_eSPI, LovyanGFX, Arduino_GFX |
| WeAct 2.9" e-paper (SSD1680) | Built-in driver (`solomon,ssd16xx`) | GxEPD2 |
| Osptek 2.13" reflective (ST7305) | No driver; port Osptek's demo init code into a small custom driver | No mainstream library; port the same demo code |
| Estardyn 2.79" bar (NV3007) | No driver; custom driver from a vendor/Arduino_GFX init sequence | Possibly Arduino_GFX (unverified) |
| LilyGO's own 1.9" + touch | n/a (part of the LilyGO board) | LilyGO examples (ST7789 parallel + CST816) |

External SPI displays can also run on the LilyGO through its spare header pins.
RAM on the nRF52840 (256 KB): use LVGL partial draw buffers for the color
panels (a full 2.8" frame is 150 KB).

## Nordic board on hand: Seeed XIAO nRF52840 Plus

- 3.3 V I/O; power displays from the **3V3** pin (never the 5V/VUSB pin for the
  ST7305 reflective panel).
- Hardware SPI on D8 (SCK) / D10 (MOSI); the extra Plus pins cover CS, DC,
  RST, BUSY and backlight.
- Built-in LiPo charging (battery pads on the back), so the Nordic prototype
  can run on battery too.
- Zephyr board target: `xiao_ble`; the extra Plus pins go in a devicetree overlay.
