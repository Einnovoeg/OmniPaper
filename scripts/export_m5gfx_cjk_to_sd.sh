#!/bin/sh
set -e
OUT_DIR="${1:-./sdcard/fonts}"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
python3 "$SCRIPT_DIR/export_m5gfx_cjk_to_sd.py" --out "$OUT_DIR"
