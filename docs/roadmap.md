# Roadmap

Two products from one device:

- **Personal:** one counter per player (or one shared per table). Bought by players.
- **Store / tournament kit:** a set of devices (e.g. 8, 16 or 32) plus a hub, a
  charging tray, and a live dashboard. Bought by stores. Smaller "friends"
  kits (e.g. 4 for a Commander pod) sit in between.

The personal device comes first. The kit shapes a few early hardware choices
(radio, battery, charging, device ID) so PCB v1 doesn't paint us into a corner.

## Phases

1. **Prototype firmware** on the LilyGO T-Display-S3 Touch, and play-test it at
   real games. *(In progress: `firmware/tdisplay-s3`.)*
2. **Pick the display and MCU** using the display test kit and battery
   measurements (see `hardware.md`).
3. **PCB v1:** MCU, display + touch, buttons, USB-C, LiPo charger, battery
   sense, a charging footprint for the tray (coil or pogo pads), test points.
   Designed for low standby current (regulator and charger quiescent current
   matter as much as the MCU's sleep current).
4. **Enclosure:** 3D printed first; injection molding only once the design settles.
5. **Configuration:** phone/web configurator, custom game profiles, sharing.
6. **Tournament kit:** hub, server and dashboard, pairings import, charging tray.

## Tournament mode (concept)

- A tournament server (laptop, Raspberry Pi or cloud) holds players, pairings
  and the round clock.
- Each device has a unique ID (from the chip) and is checked in to a player.
- At round start the server pushes table number, opponent name and a synced
  round timer to each device.
- Score changes stream to the server; a web page shows every table live.
- Both players confirm the result on the device, replacing the match slip.
- Import pairings from existing tournament software rather than replacing it.

### Radio options

| Option | Fits | Notes |
|---|---|---|
| Wi-Fi to a router the store brings | ESP32 | Simplest to build and debug. Avoid venue Wi-Fi (captive portals, flaky). Wi-Fi bursts cost battery, so send small updates and idle the radio. |
| ESP-NOW to one ESP32 hub on the laptop | ESP32 | No router, low latency, good range. |
| BLE advertising to a hub (connectionless) | nRF52 / ESP32 | Each device broadcasts its score; the hub scans, so 32 devices are fine. Pairings go out over short connections. Lowest power; keeps the Nordic option alive. |

A hub is needed in every case. Classic BLE connections to one central don't
scale to 32 devices.

## MCU decision (open)

The MCU is $1–4 of the bill of materials, so pick for headroom; the battery
budget is decided mostly by the display and the board's power design.

| | ESP32-S3 | ESP32-C3 / C6 | nRF52840 |
|---|---|---|---|
| Radios | Wi-Fi + BLE | Wi-Fi + BLE (C6 adds 802.15.4) | BLE (+ proprietary 2.4 GHz) |
| RAM / display | 512 KB + PSRAM, parallel LCD | 400–512 KB, SPI LCD | 256 KB, SPI LCD |
| Sleep | Good (~10 µA chip) | Good | Best |
| Tournament path | Wi-Fi or ESP-NOW | Wi-Fi or ESP-NOW | BLE advertising |

The counter logic (`core/`) is portable C, so either family stays possible.
Decide at PCB time based on measured battery life and which tournament radio
path proves out.

## Charging tray (open)

| Option | Pros | Cons |
|---|---|---|
| Qi receiver per device, one transmitter per tray slot | Sealed enclosure, nice UX | A transmitter per slot adds cost; heat |
| One loosely coupled resonant coil charging several devices | True "drop them on the tray" | Less standard, efficiency varies with placement |
| Pogo pins / magnetic contacts | Cheapest, efficient, simple | Exposed contacts; needs alignment |
