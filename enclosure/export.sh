#!/usr/bin/env bash
# Exports every printable part to stl/ with a revision number and a short
# description in the name, e.g. stl/shell_r06_lower-usb.stl. Refuses to
# overwrite, so each print can be traced back to the design it came from.
#
# Usage: ./export.sh <rev number> <short-description>
#        ./export.sh 6 lower-usb
set -euo pipefail
cd "$(dirname "$0")"

if [ $# -ne 2 ]; then
    echo "usage: $0 <rev number> <short-description>" >&2
    exit 2
fi
rev=$(printf "r%02d" "$1")
desc=$(echo "$2" | tr ' A-Z' '-a-z' | tr -cd 'a-z0-9-')

mkdir -p stl
for part in shell lid buttons fitcheck; do
    if ls "stl/${part}_${rev}_"*.stl >/dev/null 2>&1; then
        echo "revision $rev already exported for $part; use a new revision number" >&2
        exit 1
    fi
done
for part in shell lid buttons fitcheck; do
    out="stl/${part}_${rev}_${desc}.stl"
    openscad -q -D "part=\"$part\"" -o "$out" lilygo_case.scad
    echo "wrote $out"
done
