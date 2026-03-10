# OmniPaper User Guide

## Scope

OmniPaper is an e-paper launcher + reader + utility firmware.
Primary target: **M5PaperS3** (`m5papers3`).

## Supported Targets

- `m5papers3` (M5PaperS3) - primary/full feature set
- `m5paper` (legacy M5Paper) - supported
- `lilygo_epd47` (LilyGo T5 4.7 Plus S3) - bring-up target

## Build and Flash

Build:
- `pio run -e m5papers3`

Flash (M5PaperS3):
- `pio run -e m5papers3 --target upload --upload-port /dev/cu.usbmodemXXXX`

Other targets:
- `pio run -e m5paper --target upload --upload-port /dev/cu.usbserial-XXXX`
- `pio run -e lilygo_epd47 --target upload --upload-port /dev/cu.usbmodemXXXX`

Prebuilt binaries:
- Open the repository's GitHub Releases page for the latest firmware bundle.
- Each tagged bundle includes firmware binaries, flashing notes, notices, contributor credits, provenance,
  checksums, and the SPDX SBOM.

Web/desktop flashing tools:
- `docs/wiki/Flashing-Guide.md`

## M5PaperS3 Controls

M5PaperS3 uses touch-first controls in OmniPaper:

- Main launcher
  - Swipe to move selection
  - Tap bottom-center zone to select
- Submenus
  - Swipe up/down to move
  - Tap center zone to select
  - Tap top-right zone to go back
- Physical key
  - PaperS3 has one physical key; OmniPaper maps it as power and confirm fallback

## Core Features

- Reader: EPUB/TXT/XTC with progress and chapter navigation
- Library: recent list + file browser + cover preview
- Dashboard + Weather + PaperS3 live device telemetry
- Sensors menu: built-in + external scan + UV + diagnostics
- Network menu: Wi-Fi tools, BLE scan, web portal, host keyboard, trackpad, SSH
- Images menu: viewer + drawing
- Tools menu: notes (on-screen keyboard), file manager, time, sleep timer, OTA
- Calculator

## M5PaperS3 Hardware Integrations

- Touch: `M5.Touch.getDetail()`
- Battery/charging: `M5.Power.getBatteryLevel()`, `M5.Power.getBatteryVoltage()`, `M5.Power.getBatteryCurrent()`,
  `M5.Power.isCharging()`
- USB power sensing: `M5.Power.getVBUSVoltage()`
- RTC: `M5.Rtc.*`
- IMU (BMI270): `M5.Imu.*`
- Speaker/buzzer path: available in `Settings -> Hardware Test` speaker chirp
- SD card pins (PaperS3): `CS=47`, `SCK=39`, `MOSI=38`, `MISO=40`
- USB OTG/CDC session state shown in Dashboard and Sensors

## Web UI (Idle Hotspot)

When enabled, launcher idle mode can start a hotspot web UI.

- SSID: `OmniPaper`
- URL: `http://omnipaper.local/` (or AP IP)
- Functions: file upload/download/manage, quick app launch, open reader path

Toggle:
- `Settings -> System -> Idle Hotspot Web UI`

## SD Layout

- `/.crosspoint/` state/cache
- `/fonts/` font packs
- `/images/` image viewer files
- `/notes/` notes files
- `/games/poodle_words.txt` optional custom list
- `/logs/` diagnostics/UV logs
- `/update/` OTA update files

## Troubleshooting

- Screen not updating: power-cycle, ensure battery/USB power is stable, verify correct target (`m5papers3`) and flash succeeded.
- SD issues on PaperS3: confirm card formatting and that firmware was built with PaperS3 target.
- Missing glyphs: use SD fonts and select `Custom (SD)` in settings.
- Hardware diagnostics: `Settings -> Hardware Test`.
- PaperS3 quick health view: use `Dashboard`, `Sensors -> Built-In`, and `Hardware Test` to confirm touch, RTC, IMU,
  battery current, VBUS, USB serial state, and speaker availability.
