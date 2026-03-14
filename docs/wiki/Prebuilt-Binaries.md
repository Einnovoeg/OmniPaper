# Prebuilt Binaries

Latest prebuilt firmware release:
- Open the repository's GitHub Releases page for the latest tagged M5PaperS3 firmware bundle.
- Each tagged bundle includes `CHANGELOG.md`, generated `RELEASE_NOTES.md`, `SHA256SUMS.txt`, and the SPDX SBOM alongside the PaperS3 firmware files.

Included assets:
- `omnipaper-<tag>-m5papers3-firmware.bin`
- PaperS3 `bootloader.bin` and `partitions.bin`
- `SHA256SUMS.txt`
- `FLASHING.md`
- `BUILD_INFO.txt`
- `THIRD_PARTY_NOTICES.md`, `CONTRIBUTORS.md`, `CODE_PROVENANCE.md`, `LICENSE_COMPLIANCE.md`
- `omnipaper-<tag>.spdx.json`

Other boards remain source-build targets and are not shipped as tagged release binaries.

Verify downloads:
```bash
shasum -a 256 -c SHA256SUMS.txt
```
