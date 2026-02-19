# OmniPaper User Guide

OmniPaper is an e-paper firmware with a launcher, reader, and device tools.

## Supported Boards

- `m5paper`: M5Stack M5Paper (ESP32) - full feature set
- `m5papers3`: M5Stack M5PaperS3 (ESP32-S3) - full feature set
- `lilygo_epd47`: LilyGo T5 4.7 Plus S3 - bring-up build (serial/AP/SD baseline)

## Build and Flash

Build:
- `pio run -e m5paper`
- `pio run -e m5papers3`
- `pio run -e lilygo_epd47`

Flash:
- `pio run -e m5paper --target upload --upload-port /dev/cu.usbserial-XXXX`
- `pio run -e m5papers3 --target upload --upload-port /dev/cu.usbmodemXXXX`
- `pio run -e lilygo_epd47 --target upload --upload-port /dev/cu.usbmodemXXXX`

## 1. Quick Start

1. Insert SD card.
2. Boot device.
3. Open `Reader` from the launcher.
4. Choose a book from `Recent` or `Files`.

Supported book formats:
- `.epub`
- `.txt`, `.md`
- `.xtc`, `.xtch`

## 2. Launcher Apps

Main launcher includes:
- Reader
- Dashboard
- Sensors
- Weather
- Network
- Games
- Images
- Tools
- Settings
- Calculator

### Network Submenu

Network tools now include:
- `WiFi Status`
- `WiFi Scan`
- `WiFi Tests`
- `Channels`
- `BLE Scan`
- `Web Portal` (manual file-transfer portal in STA/AP mode)
- `Host Keyboard` (BLE HID keyboard + USB serial text mode)
- `Trackpad` (BLE HID mouse/scroll using touch gestures)
- `SSH Client` (WiFi + SSH command execution)

## 3. Library and Picker

## Tabs
- `Recent`: recently opened books with metadata.
- `Files`: file/folder browser.

## EPUB Picker with Cover Art
The `Files` tab shows a cover preview panel for the selected book when available.
- EPUB: uses cached thumbnail/cover from book metadata.
- XTC/XTCH: uses generated thumbnail/cover.
- TXT/MD: uses nearby cover image if available.

## 4. Reader

## Navigation
- `Left/Right`: previous/next page.
- `Confirm`: reader menu.
- `Back`: return to library.

## Chapter and Progress
- Chapter menu and chapter jump supported.
- Reading progress saved automatically.

## Embedded Images in EPUB
Inline EPUB images are supported in reading flow.
- Supported inline source types: JPEG and BMP.
- Unsupported image types are shown as a placeholder label.

## 5. Fonts and UTF-8

## User Provided Fonts
Fonts are loaded from SD (`/fonts/*.epf`).
You can provide a custom reader family and select it from Settings (`Custom (SD)`).

Expected custom files:
- `user_12_regular.epf`
- `user_12_bold.epf`
- `user_12_italic.epf`
- `user_12_bolditalic.epf`
- `user_14_regular.epf`
- `user_16_regular.epf`
- `user_18_regular.epf`

## Full UTF Support
Rendering path is UTF-8 end-to-end.
Displayed character coverage depends on installed font glyphs.
For broad multilingual coverage, install Unicode-capable SD font packs.

## 6. Settings Highlights

Reader settings include:
- Font family (Bookerly, Noto Sans, Open Dyslexic, Custom SD)
- Font size
- Line spacing
- Paragraph alignment
- Hyphenation
- Orientation
- Margins

System settings include:
- Sleep timeout
- Idle Hotspot Web UI (auto-host AP web portal on launcher)
- OTA update
- Cache controls
- Network/account options

Display settings include:
- Status bar mode
- Info Overlay position: `Off`, `Top`, `Bottom`, `Top Corners`, `Bottom Corners`
  - Shows battery, WiFi, and RTC-backed date/time

Power/sleep behavior:
- Short power-button action is configurable in Controls settings.
- Long power-button press enters sleep.
- Timed sleep uses M5Unified power deep-sleep API with wake support.

## 7. Idle Hotspot Web UI

When enabled, the launcher can automatically host a hotspot web portal:
- SSID: `OmniPaper`
- URL: `http://omnipaper.local/` (or AP IP fallback)

Available from the web UI:
- SD file management (`/files`) with upload/download/create/delete
- Quick launch of selected apps
- Open an EPUB directly by SD path
  - Quick-launch app IDs include `host-keyboard`, `trackpad`, and `ssh-client`

Toggle location:
- `Settings -> System -> Idle Hotspot Web UI`

## 8. SD Card Layout

Common paths:
- `/.crosspoint/` internal cache and state
- `/fonts/` font packs
- `/images/` image viewer source images
- `/notes/` notes app data
- `/games/poodle_words.txt` optional custom Poodle word list
- `/logs/` generated diagnostics and UV logs
- `/update/` OTA-from-SD firmware files

## 9. Troubleshooting

- Missing glyphs: install wider Unicode font pack and/or use `Custom (SD)` family.
- Missing cover art in picker: open the book once to populate cache metadata/thumbnail.
- EPUB image missing: image type may be unsupported (PNG/SVG/GIF currently shown as placeholder).
- Slow rendering in image-heavy EPUB: expected due e-paper refresh and SD image decode.
- Hardware validation: open `Settings -> Hardware Test` to verify buttons (including power), touch, RTC time, SD, and SHT30 readout.
