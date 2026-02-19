#!/usr/bin/env python3
import argparse
import codecs
import re
import sys
from pathlib import Path

ARRAY_RE = re.compile(r"PROGMEM\s+const\s+uint8_t\s+(\w+)\s*\[\s*[^\]]*\]\s*=", re.M)
STRING_RE = re.compile(r"\"((?:\\\\.|[^\"])*)\"")


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*", "", text)
    return text


def parse_arrays(path: Path):
    text = path.read_text()
    text = _strip_comments(text)
    arrays = []
    for match in ARRAY_RE.finditer(text):
        name = match.group(1)
        start = match.end()
        end = text.find(";\n", start)
        if end == -1:
            continue
        raw = text[start:end]
        parts = STRING_RE.findall(raw)
        if parts:
            data = bytearray()
            for part in parts:
                decoded = codecs.decode(part, "unicode_escape")
                data.extend(decoded.encode("latin1"))
            arrays.append((name, data))
            continue

        # Fallback for numeric array literals
        tokens = [tok.strip() for tok in raw.replace("\n", " ").split(",") if tok.strip()]
        data = bytearray()
        for tok in tokens:
            if not tok:
                continue
            try:
                if tok.lower().startswith("0x"):
                    value = int(tok, 16)
                else:
                    value = int(tok, 10)
            except ValueError:
                continue
            if value < 0 or value > 255:
                raise ValueError(f"Invalid byte value {value} in {path}")
            data.append(value & 0xFF)
        if data:
            arrays.append((name, data))
    return arrays


def find_m5gfx_fonts_root(project_root: Path) -> Path:
    candidate = project_root / ".pio" / "libdeps" / "m5paper" / "M5GFX" / "src" / "lgfx" / "Fonts"
    return candidate


def export_arrays(arrays, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for name, data in arrays:
        out_path = out_dir / f"{name}.u8g2"
        out_path.write_bytes(data)
        count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description="Export M5GFX CJK fonts to SD card files.")
    parser.add_argument("--out", default="sdcard/fonts", help="Output directory for .u8g2 files")
    parser.add_argument("--m5gfx-fonts", default=None, help="Path to M5GFX 'Fonts' directory")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    fonts_root = Path(args.m5gfx_fonts) if args.m5gfx_fonts else find_m5gfx_fonts_root(project_root)

    if not fonts_root.exists():
        print("M5GFX Fonts directory not found:")
        print(f"  {fonts_root}")
        print("Build once for m5paper to populate .pio/libdeps, or pass --m5gfx-fonts.")
        return 1

    sources = [
        fonts_root / "IPA" / "lgfx_font_japan.c",
        fonts_root / "efont" / "lgfx_efont_ja.c",
        fonts_root / "efont" / "lgfx_efont_cn.c",
        fonts_root / "efont" / "lgfx_efont_kr.c",
        fonts_root / "efont" / "lgfx_efont_tw.c",
    ]

    all_arrays = []
    for src in sources:
        if not src.exists():
            print(f"Missing source file: {src}")
            continue
        all_arrays.extend(parse_arrays(src))

    if not all_arrays:
        print("No font arrays found.")
        return 1

    out_dir = Path(args.out)
    count = export_arrays(all_arrays, out_dir)
    print(f"Exported {count} fonts to {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
