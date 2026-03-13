#!/usr/bin/env python3
"""Extract release notes for a tagged OmniPaper version from CHANGELOG.md."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CHANGELOG_PATH = ROOT / "CHANGELOG.md"
VERSION_HEADING_RE = re.compile(r"^## \[(?P<version>[^\]]+)\](?: - (?P<date>\d{4}-\d{2}-\d{2}))?$")


def normalize_version(raw_version: str) -> str:
    return raw_version.strip().removeprefix("v").strip()


def extract_release_section(version: str, changelog_text: str) -> str:
    lines = changelog_text.splitlines()
    normalized_version = normalize_version(version)
    start_index: int | None = None

    for index, line in enumerate(lines):
        match = VERSION_HEADING_RE.match(line.strip())
        if match and normalize_version(match.group("version")) == normalized_version:
            start_index = index
            break

    if start_index is None:
        raise ValueError(f"Unable to find changelog entry for version {version}")

    body_lines: list[str] = []
    for line in lines[start_index + 1 :]:
        if line.startswith("## "):
            break
        body_lines.append(line)

    body = "\n".join(body_lines).strip()
    if not body:
        raise ValueError(f"Changelog entry for version {version} is empty")

    return f"# OmniPaper {normalized_version}\n\n{body}\n"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="release version or tag name")
    parser.add_argument(
        "--output",
        type=Path,
        help="optional output path; prints to stdout when omitted",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    changelog_text = CHANGELOG_PATH.read_text(encoding="utf-8")
    notes = extract_release_section(args.version, changelog_text)

    if args.output is None:
        sys.stdout.write(notes)
        return 0

    output_path = args.output
    if not output_path.is_absolute():
        output_path = ROOT / output_path
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(notes, encoding="utf-8")
    print(f"Wrote release notes to {output_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
