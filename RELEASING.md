# Releasing OmniPaper

This repository ships tagged firmware bundles for the stable M5 targets.

The release tag must match the current `platformio.ini` version exactly, without a leading `v`. The same bare semantic version must appear in `CHANGELOG.md` so OTA version checks, artifact names, and GitHub Releases all stay consistent.

## Release Gate

Before tagging a release:

1. Add or update the matching `CHANGELOG.md` entry for the current version.
2. Run:

```bash
python3 scripts/compliance.py check --release
python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json
python3 scripts/extract_changelog.py --version "$(python3 - <<'PY'
import configparser
parser = configparser.ConfigParser(interpolation=None, strict=False)
parser.read("platformio.ini", encoding="utf-8")
print(parser.get("omnipaper", "version").strip())
PY
)" --output build/release-notes.md
pio run -e m5papers3_release
pio run -e m5paper_release
```

3. Create an annotated tag that matches the version, for example `git tag -a 0.17.0 -m "OmniPaper 0.17.0"`.
4. Push the commit and tag with `git push origin main --follow-tags`.

## Release Bundle Contents

The tag workflow packages and publishes:

- `omnipaper-<tag>-m5papers3-firmware.bin`
- `omnipaper-<tag>-m5paper-firmware.bin`
- board-specific `bootloader.bin`, `partitions.bin`, `firmware.elf`, and `firmware.map`
- `README.md`
- `LICENSE`
- `CHANGELOG.md`
- `RELEASE_NOTES.md`
- `DEPENDENCIES.md`
- `CODE_PROVENANCE.md`
- `CONTRIBUTORS.md`
- `THIRD_PARTY_NOTICES.md`
- `LICENSE_COMPLIANCE.md`
- `FLASHING.md`
- `SHA256SUMS.txt`
- `BUILD_INFO.txt`
- `omnipaper-<tag>.spdx.json`

## Notes

- `m5papers3_release` is the primary release target.
- `lilygo_epd47` remains a bring-up target and is not bundled by default in tagged releases.
- OTA release checks use the repository slug detected at build time from CI or the local `origin` remote, so release builds point at the correct OmniPaper GitHub Releases feed without hardcoding a maintainer identity in source files.
- The workflow uploads the assembled bundle as a GitHub Actions artifact named `OmniPaper-<tag>` and publishes the same files to the matching GitHub Release.
