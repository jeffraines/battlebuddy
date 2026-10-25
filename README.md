# BattleBuddy

A small, cheap, dedicated score/life counter for tabletop card games
(Riftbound, Magic, Lorcana, Flesh and Blood, …), so nobody has to use a phone,
paper, or a fiddly token track.

## Goals

- **Dedicated device.** No phone on the table, no notifications.
- **Multi-game.** Each game is a *profile* (starting value, count up or down,
  win/loss threshold, extra counters). New games ship as data, not firmware.
- **Buy once.** New profiles and firmware updates over Bluetooth (or USB).
- **Personal or shared.** One player's counter, or one device between both
  players with each half facing its player.
- **Fewer disputes.** A history log with undo, which paper and token tracks
  don't have.
- **Cheap.** USB-C charging, LiPo battery, a hard power button.

## Status

Prototyping. The first working prototype runs on the LilyGO T-Display-S3 Touch:
see [firmware/tdisplay-s3](firmware/tdisplay-s3).

## Layout

- `core/`: portable C counter logic (game profiles, grouped changes, undo
  history) with host tests (`make -C core test`).
- `firmware/tdisplay-s3/`: PlatformIO firmware for the T-Display-S3 Touch.
- `docs/`: hardware notes and game profile format.
- `tools/`: printable display size cutouts.

See [docs/hardware.md](docs/hardware.md) for the
hardware decisions and open questions, and [docs/game-profiles.md](docs/game-profiles.md)
for how games are described.
