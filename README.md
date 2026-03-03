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
- M5PaperS3 SD SPI pin map update
  - `CS=47`, `SCK=39`, `MOSI=38`, `MISO=40`
- PaperS3 input model update
  - single physical power key mapped for UI fallback
  - touch virtual keys + swipe navigation for launcher/submenus
- RTC/battery/touch/IMU paths validated against M5Unified APIs
  - `M5.Rtc.*`, `M5.Power.*`, `M5.Touch.*`, `M5.Imu.*`
- USB OTG/CDC awareness in UI (dashboard status)
- Sleep behavior adjusted for PaperS3 wake reliability

## What OmniPaper Does

- Reader: EPUB/TXT/XTC with progress/chapter navigation
- Library: recent + file browser with EPUB cover panel
- Launcher: icon-grid boot menu with app categories
- Network tools: Wi-Fi scan/status/tests/channels, BLE scan, SSH client, web portal, host keyboard, trackpad
- Sensors/tools: built-in/external sensor views, I2C tools, UV sensor/logs, hardware test
- Apps: weather, dashboard, notes + on-screen keyboard, drawing, image viewer, calculator, file manager
- Games: Poodle, Sudoku
- Idle hotspot web UI (toggle in settings) for file transfer and app launch

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
- <https://github.com/<OWNER_OR_ORG>/OmniPaper/releases/latest>
- Replace `<OWNER_OR_ORG>` with your GitHub account or organization name.

Flashing tools/instructions (web + desktop):
- `docs/wiki/Flashing-Guide.md`

## Dependencies

- Build/runtime dependency list: `DEPENDENCIES.md`
- Compliance process/checklist: `LICENSE_COMPLIANCE.md`
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

- Project license: MIT (`LICENSE`)
- Third-party notices/licenses: `THIRD_PARTY_NOTICES.md`

