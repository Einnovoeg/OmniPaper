# License Compliance Guide

This document defines how OmniPaper satisfies third-party license obligations and how releases are checked before firmware artifacts are published.

## 1. Source Attribution Files

- `CODE_PROVENANCE.md`: exact source repositories and local integration paths.
- `THIRD_PARTY_NOTICES.md`: full third-party notices and license texts.
- `CONTRIBUTORS.md`: upstream contributor credit links for direct code sources and referenced projects.
- `DEPENDENCIES.md`: build-time and runtime dependency inventory.
- `LICENSE`: OmniPaper project license (MIT).
- `RELEASING.md`: release bundle contents and release-gate commands.

## 2. Direct Code Sources

OmniPaper includes or adapts code from these upstream projects:

- CrossPoint-reader (`daveallie/crosspoint-reader`) - MIT
- open-x4 community SDK (`open-x4-epaper/community-sdk`) - MIT
- LilyGo-EPD47 (`Xinyuan-LilyGO/LilyGo-EPD47`) - MIT
- M5Unified (`m5stack/M5Unified`) - MIT
- M5GFX (`m5stack/M5GFX`) - MIT
- Arduino-ESP32 Core (`espressif/arduino-esp32`) - LGPL-2.1-or-later
- AS7331 (`RobTillaart/AS7331`) - MIT
- ArduinoJson (`bblanchon/ArduinoJson`) - MIT
- QRCode (`ricmoo/QRCode`) - MIT
- arduinoWebSockets (`Links2004/arduinoWebSockets`) - LGPL-2.1
- SdFat (`greiman/SdFat`) - MIT
- LibSSH-ESP32 (`ewpa/LibSSH-ESP32`) - LGPL-2.1 (+ BSD parts)
- Expat (`libexpat/libexpat`) - MIT
- miniz (`richgel999/miniz`) - MIT
- picojpeg (`richgel999/picojpeg`) - Public Domain
- EPDiy structures/scripts (`vroland/epdiy`) - LGPL-3.0 upstream license

All direct adaptation points are documented in `CODE_PROVENANCE.md` and, where useful, in local source comments.

## 3. Dependency License Handling

- MIT, BSD, Apache, Unlicense, Public Domain components: retain attribution and notices in `THIRD_PARTY_NOTICES.md`.
- LGPL components (`Arduino-ESP32 Core`, `arduinoWebSockets`, `LibSSH-ESP32`):
  - keep license texts in the repository,
  - keep corresponding source references in `CODE_PROVENANCE.md`,
  - keep upstream contributor credit links in `CONTRIBUTORS.md`,
  - ship release bundles with the notices and source-reference documents.
- Font licenses (OFL/UFL): keep attribution and license text notices and do not relabel font licensing terms.

## 4. No-License Sources Policy

Repositories or gists with no explicit code license are treated as reference-only:

- No source files are copied from those repositories.
- OmniPaper contains independent local implementations.
- The reference source is still credited in `CODE_PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`, and `CONTRIBUTORS.md`.

## 5. Automated Checks

Local and CI compliance verification is centralized in `scripts/compliance.py`.

Repository check:

```bash
python3 scripts/compliance.py check
```

Release-mode check:

```bash
python3 scripts/compliance.py check --release
```

SPDX SBOM generation:

```bash
python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json
```

The checker currently verifies:

- required compliance documents are present,
- first-party tracked files do not contain blocked personal identifiers or owner placeholders,
- `platformio.ini` dependencies are documented in `CODE_PROVENANCE.md`,
- included third-party components are covered by `THIRD_PARTY_NOTICES.md`,
- included third-party components and contributor-graph links are covered by `CONTRIBUTORS.md`.

## 6. Release Checklist

Before publishing firmware binaries:

1. Run `python3 scripts/compliance.py check --release`.
2. Generate the SPDX SBOM with `python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json`.
3. Confirm `CODE_PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`, and `CONTRIBUTORS.md` reflect any newly copied, adapted, or referenced third-party code.
4. Confirm release artifacts include `LICENSE`, `DEPENDENCIES.md`, `CODE_PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`, `CONTRIBUTORS.md`, `LICENSE_COMPLIANCE.md`, `FLASHING.md`, `SHA256SUMS.txt`, `BUILD_INFO.txt`, and the generated SPDX SBOM.
5. Confirm the published binaries still correspond to source available in this repository for LGPL-covered components.

## 7. Verification Commands

Typical local verification:

```bash
python3 scripts/compliance.py check
python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json
pio run -e m5papers3
pio run -e m5paper
```
