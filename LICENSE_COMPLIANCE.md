# License Compliance Guide

This document defines how OmniPaper satisfies third-party license obligations.

## 1. Source Attribution Files

- `CODE_PROVENANCE.md`: exact source repositories and local integration paths.
- `THIRD_PARTY_NOTICES.md`: full third-party notices and license texts.
- `CONTRIBUTORS.md`: contributor credit links for upstream projects.
- `LICENSE`: OmniPaper project license (MIT).

## 2. Direct Code Sources (Included or Adapted)

OmniPaper includes or adapts code from these upstream projects:

- CrossPoint-reader (`daveallie/crosspoint-reader`) - MIT
- open-x4 community SDK (`open-x4-epaper/community-sdk`) - MIT
- LibSSH-ESP32 (`ewpa/LibSSH-ESP32`) - LGPL-2.1 (+ BSD parts)
- Expat (`libexpat/libexpat`) - MIT
- miniz (`richgel999/miniz`) - MIT
- picojpeg (`richgel999/picojpeg`) - Public Domain
- EPDiy structures/scripts (`vroland/epdiy`) - LGPL-3.0 upstream license

All direct adaptation points are documented in `CODE_PROVENANCE.md` and with per-file comments where applicable.

## 3. Dependency License Handling

- MIT/BSD/Apache/Unlicense/Public Domain components: attribution + notice retention in `THIRD_PARTY_NOTICES.md`.
- LGPL components (`arduinoWebSockets`, `Arduino-ESP32`, `LibSSH-ESP32`):
  - keep license texts in repository notices,
  - provide corresponding source references and dependency versions,
  - document distribution obligations for binary firmware releases.
- Font licenses (OFL/UFL): keep attribution and license text notices; do not relabel font licensing terms.

## 4. No-License Sources Policy

Repositories or gists with no explicit code license are treated as **reference-only**:

- No source files are copied from those repositories.
- OmniPaper contains independent local implementations.
- The reference source is still credited in provenance and notices.

## 5. Release Checklist

Before publishing firmware releases:

1. Confirm `CODE_PROVENANCE.md` is up to date for new copied/adapted code.
2. Confirm `THIRD_PARTY_NOTICES.md` includes applicable notices and license text.
3. Confirm `CONTRIBUTORS.md` includes upstream contributor links for newly used projects.
4. Confirm binaries/release notes link back to this repository source.
5. Confirm no unlicensed third-party code was copied into the tree.

## 6. Verification Commands

Run these checks before release:

```bash
# personal identifier scan (repo-specific)
rg -n --hidden --glob '!.git' --glob '!.pio' '/Users/|/Volumes/'

# attribution coverage quick scan
rg -n 'github.com/' CODE_PROVENANCE.md THIRD_PARTY_NOTICES.md CONTRIBUTORS.md

# local build verification
pio run -e m5papers3
```
