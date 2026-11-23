#!/usr/bin/env bash
# Builds the firmware and merges bootloader + partition table + app into one
# file that flashes at offset 0x0 (for the browser flasher or esptool).
# Usage: ./make_flash_image.sh [output dir]   (default: ./release)
set -euo pipefail
cd "$(dirname "$0")"
out=${1:-release}
pio run
b=.pio/build/tdisplay-s3
app0=$(find ~/.platformio/packages -name boot_app0.bin | head -1)
esptool=$(find ~/.platformio/packages -name esptool.py | head -1)
mkdir -p "$out"
file="$out/battlebuddy-tdisplay-s3-$(git log -1 --format=%h).bin"
python3 "$esptool" --chip esp32s3 merge_bin -o "$file" --flash_mode dio --flash_size 16MB \
    0x0 "$b/bootloader.bin" 0x8000 "$b/partitions.bin" 0xe000 "$app0" 0x10000 "$b/firmware.bin"
echo "wrote $file"
