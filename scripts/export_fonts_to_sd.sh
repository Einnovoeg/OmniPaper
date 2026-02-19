#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-sdcard/fonts}"
mkdir -p "$OUT_DIR"

CXX="${CXX:-g++}"
$CXX -std=c++17 -O2 -I lib/EpdFont -I lib/EpdFont/builtinFonts tools/fontdump/fontdump.cpp -o /tmp/crosspoint-fontdump

/tmp/crosspoint-fontdump "$OUT_DIR"

echo "Exported fonts to: $OUT_DIR"
