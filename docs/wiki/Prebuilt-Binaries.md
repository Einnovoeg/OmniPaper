# Prebuilt Binaries

Latest prebuilt firmware release:
- Open the repository's GitHub Releases page for the latest tagged firmware bundle.

Included assets:
- `omnipaper-<tag>-m5paper-firmware.bin`
- `omnipaper-<tag>-m5papers3-firmware.bin`
- board-specific `bootloader.bin` and `partitions.bin`
- `SHA256SUMS.txt`
- `FLASHING.md`
- `BUILD_INFO.txt`
- `THIRD_PARTY_NOTICES.md`, `CONTRIBUTORS.md`, `CODE_PROVENANCE.md`, `LICENSE_COMPLIANCE.md`
- `omnipaper-<tag>.spdx.json`

Verify downloads:
```bash
shasum -a 256 -c SHA256SUMS.txt
```
