# OmniPaper

OmniPaper is an all-in-one e-paper firmware derived from **CrossPoint-reader** and adapted/extended for modern M5 e-paper boards, with **M5PaperS3 as the primary target**.

This project directly uses code from upstream projects (not just "inspiration"). See:
- `CODE_PROVENANCE.md` for file/component sourcing
- `THIRD_PARTY_NOTICES.md` for license notices and legal text
- `CONTRIBUTORS.md` for contributor and project credits

## Primary Target: M5PaperS3

OmniPaper now defaults to the `m5papers3` PlatformIO environment.

### M5PaperS3-Specific Hardware/Programming Changes Implemented

Compared with legacy M5Paper, M5PaperS3 migration includes:
- ESP32-S3 target and custom board definition (`boards/m5stack_papers3.json`)
- M5Unified fallback board lock to `board_M5PaperS3`
- Correct PaperS3 panel geometry handling
  - physical panel: `960x540`
  - PaperS3 UI path: `540x960` logical portrait layout
- M5PaperS3 SD SPI pin map update
  - `CS=47`, `SCK=39`, `MOSI=38`, `MISO=40`
- PaperS3 input model update
  - single physical power key mapped for UI fallback
  - direct tap selection for launcher tiles, submenu rows, and settings rows
  - swipe-to-select removed from the PaperS3 launcher path
- PaperS3 portrait UI update
  - larger header/footer typography
  - large rectangular touch targets and square launcher icon boxes
  - portrait-first launcher, settings, dashboard, sensors, and hardware test screens
- RTC/battery/touch/IMU paths validated against M5Unified APIs
  - `M5.Rtc.*`, `M5.Power.*`, `M5.Touch.*`, `M5.Imu.*`
- USB OTG/TinyUSB device-mode configuration for PaperS3
  - CDC console support on boot
  - UI reports both USB cable presence (`VBUS`) and CDC session state
- Sleep behavior adjusted for PaperS3 wake reliability

### PaperS3 USB OTG Scope

What is enabled in OmniPaper right now:
- TinyUSB USB-OTG device mode for the PaperS3 firmware target
- USB CDC console on boot
- On-device diagnostics for cable presence (`VBUS`) and CDC session state

What is not enabled in this release:
- USB host-mode peripherals such as cameras, USB mass-storage host mounting, or HID host stacks

The ESP32-S3 silicon and TinyUSB stack can support broader USB roles, but this firmware currently ships only the stable device-mode path.

## What OmniPaper Does

- Reader: EPUB/TXT/XTC with progress/chapter navigation
- Library: recent + file browser with EPUB cover panel
- Launcher: icon-grid boot menu with app categories
- Network tools: Wi-Fi scan/status/tests/channels, BLE scan, SSH client, web portal, host keyboard
- Sensors/tools: built-in/external sensor views, I2C tools, UV sensor/logs, hardware test
- Apps: weather, dashboard, notes + on-screen keyboard, drawing, image viewer, file manager, trackpad
- Calculator: scientific keypad tuned for direct touch on PaperS3
- Games: Poodle, Sudoku, Tetris
- Idle hotspot web UI (toggle in settings) for file transfer and app launch

## Install And Build

### 1. Prerequisites

- Git
- Python 3.10+
- PlatformIO Core 6.x (`pio`)
- A USB data cable for the target board

### 2. Get the source

```bash
git clone <your-fork-or-mirror-url>
cd OmniPaper
```

### 3. Build the primary PaperS3 target

```bash
pio run -e m5papers3
```

### 4. Flash the board

```bash
pio run -e m5papers3 --target upload --upload-port /dev/cu.usbmodemXXXX
```

After flashing, the launcher should boot into the portrait, direct-touch PaperS3 UI.

## Board Support

| Board | PlatformIO env | Status |
|---|---|---|
| M5Stack M5PaperS3 | `m5papers3` | Primary target (full feature set) |
| M5Stack M5Paper | `m5paper` | Supported |
| LilyGo T5 4.7 Plus S3 | `lilygo_epd47` | Bring-up target |

## Build

```bash
pio run -e m5papers3
```

Other targets:

```bash
pio run -e m5paper
pio run -e lilygo_epd47
```

## Flash

M5PaperS3:

```bash
pio run -e m5papers3 --target upload --upload-port /dev/cu.usbmodemXXXX
```

M5Paper:

```bash
pio run -e m5paper --target upload --upload-port /dev/cu.usbserial-XXXX
```

LilyGo-EPD47:

```bash
pio run -e lilygo_epd47 --target upload --upload-port /dev/cu.usbmodemXXXX
```

Prebuilt binaries and release assets:
- Open the repository's GitHub Releases page for the latest tagged **M5PaperS3** firmware bundle.
- Release bundles include the PaperS3 firmware image, bootloader/partition files, `CHANGELOG.md`,
  generated `RELEASE_NOTES.md`, `THIRD_PARTY_NOTICES.md`, `CONTRIBUTORS.md`, `CODE_PROVENANCE.md`,
  `LICENSE_COMPLIANCE.md`, and an SPDX SBOM.

Flashing tools/instructions (web + desktop):
- `docs/wiki/Flashing-Guide.md`

## Verification

Recommended local verification before publishing:

```bash
python3 scripts/compliance.py check --release
python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json
pio run -e m5papers3_release
./test/run_hyphenation_eval.sh
```

Optional legacy regression:

```bash
pio run -e m5paper_release
```

## Versioning And Releases

- OmniPaper uses bare semantic versions such as `0.18.0`; the Git tag, `platformio.ini` version, and `CHANGELOG.md` entry must match exactly.
- Tagged GitHub releases now publish a single merged M5PaperS3 firmware image, `omnipaper-<tag>-m5papers3.bin`, as the primary supported release artifact.
- Other targets remain buildable from source, but the public release process is centered on the PaperS3.
- OTA release checks compile the repository slug from CI metadata or the local `origin` remote, so official release builds point at the correct OmniPaper GitHub release feed without committing maintainer-specific identifiers to the repository.

## Dependencies

- Build/runtime dependency list: `DEPENDENCIES.md`
- Compliance process/checklist: `LICENSE_COMPLIANCE.md`
- Release runbook: `RELEASING.md`
- Compliance checker: `python3 scripts/compliance.py check`
- SPDX SBOM generation: `python3 scripts/compliance.py sbom --output build/omnipaper.spdx.json`
- Tooling is managed by PlatformIO from `platformio.ini`

## Attribution And License Compliance

- This firmware includes and adapts third-party code from the repositories listed in `CODE_PROVENANCE.md`.
- Full third-party legal notices and license texts are provided in `THIRD_PARTY_NOTICES.md`.
- Upstream contributor credits are listed in `CONTRIBUTORS.md`.
- Components with no explicit upstream code license are treated as **reference-only** sources; no direct code copying from those sources is included in this repository.

## Source Credits (Exact Repositories)

Core derivation and major code sources:
- CrossPoint-reader: <https://github.com/daveallie/crosspoint-reader>
- open-x4 community SDK: <https://github.com/open-x4-epaper/community-sdk>
- M5Unified: <https://github.com/m5stack/M5Unified>
- M5GFX: <https://github.com/m5stack/M5GFX>
- LilyGo-EPD47: <https://github.com/Xinyuan-LilyGO/LilyGo-EPD47>

App/game/tool source references used in this project are enumerated in:
- `CODE_PROVENANCE.md`

## License

- Project license: MIT (`LICENSE`) for OmniPaper first-party code
- Third-party notices/licenses: `THIRD_PARTY_NOTICES.md`

## Support

- Buy Me a Coffee: [buymeacoffee.com/einnovoeg](https://buymeacoffee.com/einnovoeg)
