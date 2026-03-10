# Releasing OmniPaper

This repository ships tagged firmware bundles for the stable M5 targets.

## Release Gate

Before tagging a release, run:

```bash
python3 scripts/compliance.py check --release
python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json
pio run -e m5papers3_release
pio run -e m5paper_release
```

## Release Bundle Contents

The tag workflow packages:

- `omnipaper-<tag>-m5papers3-firmware.bin`
- `omnipaper-<tag>-m5paper-firmware.bin`
- board-specific `bootloader.bin`, `partitions.bin`, `firmware.elf`, and `firmware.map`
- `README.md`
- `LICENSE`
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
- The workflow uploads the assembled bundle as a GitHub Actions artifact named `OmniPaper-<tag>`.
