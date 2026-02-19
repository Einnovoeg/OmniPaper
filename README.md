# OmniPaper

OmniPaper is a rewritten and enhanced e-paper firmware derived from **CrossPoint-reader** and adapted for multiple ESP32 e-paper targets.

This project, named **OmniPaper**, is derived from **CrossPoint-reader et al.**, then adapted and extended for additional hardware and apps.

## Derivation

This project, **OmniPaper**, is derived from and directly uses code from:
- CrossPoint Reader: <https://github.com/daveallie/crosspoint-reader>
- open-x4 community SDK: <https://github.com/open-x4-epaper/community-sdk>

## CrossPoint-reader Enhancements in This Fork

This firmware is **CrossPoint-reader adapted and enhanced**. Main enhancements include:
- Multi-target PlatformIO environments (`m5paper`, `m5papers3`, `lilygo_epd47`)
- Boot icon-grid launcher with app/category menus
- EPUB file picker cover preview panel
- Inline EPUB image rendering (JPEG/BMP pipeline)
- SD-offloaded/custom font families (`/fonts/user_*.epf`)
- UTF-8 rendering path and UTF-8 SD long-filename support
- Sensor/utility suite (including AS7331 UV app and logging)
- Added apps: image viewer, drawing app, notes app with on-screen keyboard, calculator, diagnostics tools
- Added games: Poodle and Sudoku integrations
- Expanded network tools (Wi-Fi scanner/status/tests/channel monitor, BLE scanner)
- Idle hotspot web UI mode with file transfer and web quick-launch actions
- RTC-backed date/time overlay with configurable placement (top/bottom/corners)
- M5Paper power-button mapping and deep-sleep wake flow via M5Unified Power API

## Code Provenance

Exact component-level code sourcing is documented in:
- [`CODE_PROVENANCE.md`](CODE_PROVENANCE.md)

## Board Support

| Board | PlatformIO env | Status |
|---|---|---|
| M5Stack M5Paper (ESP32) | `m5paper` | Full OmniPaper feature set |
| M5Stack M5PaperS3 (ESP32-S3) | `m5papers3` | Full OmniPaper feature set |
| LilyGo T5 4.7 Plus S3 (LilyGo-EPD47) | `lilygo_epd47` | Bring-up build (serial + WiFi AP + SD baseline) |

Notes:
- `lilygo_epd47` currently targets board bring-up and platform validation while display/input integration is expanded.
- Single PlatformIO configuration file is maintained at `platformio.ini`.

## Feature Status

- [x] User provided fonts
- [x] Full UTF support (UTF-8 rendering path; glyph coverage depends on installed fonts)
- [x] EPUB picker with cover art
- [x] Image support within EPUB (inline JPEG/BMP)
- [x] Idle hotspot web UI toggle in Settings

## Build

```bash
pio run -e m5paper
pio run -e m5papers3
pio run -e lilygo_epd47
```

## Flashing

M5Paper:
```bash
pio run -e m5paper --target upload --upload-port /dev/cu.usbserial-XXXX
```

M5PaperS3:
```bash
pio run -e m5papers3 --target upload --upload-port /dev/cu.usbmodemXXXX
```

LilyGo-EPD47:
```bash
pio run -e lilygo_epd47 --target upload --upload-port /dev/cu.usbmodemXXXX
```

Optional diagnostics:
```bash
pio run -e m5paper_epd_diag --target upload --upload-port /dev/cu.usbserial-XXXX
pio run -e m5paper_alive_diag --target upload --upload-port /dev/cu.usbserial-XXXX
```

Web-based and downloadable flashing tools (macOS/Linux/Windows):
- [`docs/wiki/Flashing-Guide.md`](docs/wiki/Flashing-Guide.md)
- Prebuilt binaries: <https://github.com/Einnovoeg/OmniPaper/releases/latest>

## New Repository Prep

For publishing to a new GitHub repository:
- Keep `LICENSE` (project MIT license).
- Keep `THIRD_PARTY_NOTICES.md` (full notices and legal text).
- Keep `CODE_PROVENANCE.md` (source-level attribution map).
- Keep `README.md` concise and point to legal/provenance docs.
- Preserve upstream repository links listed in this README and provenance docs.

## SD Fonts

Export SD font assets:

```bash
./scripts/export_fonts_to_sd.sh
```

Optional custom reader family files in `/fonts/`:
- `user_12_regular.epf`
- `user_12_bold.epf`
- `user_12_italic.epf`
- `user_12_bolditalic.epf`
- `user_14_regular.epf`
- `user_16_regular.epf`
- `user_18_regular.epf`

Then select `Custom (SD)` in Reader font settings.

## Documentation

- User guide: [`USER_GUIDE.md`](USER_GUIDE.md)
- Code provenance map: [`CODE_PROVENANCE.md`](CODE_PROVENANCE.md)
- Third-party notices and full legal text: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- Wiki pages (repo-hosted): [`docs/wiki/Home.md`](docs/wiki/Home.md)
- Prebuilt binaries release: <https://github.com/Einnovoeg/OmniPaper/releases/tag/v0.17.0>

## Third-Party Projects

Libraries/frameworks and upstream projects used in this firmware include:
- CrossPoint Reader: <https://github.com/daveallie/crosspoint-reader>
- open-x4 community SDK: <https://github.com/open-x4-epaper/community-sdk>
- M5Unified: <https://github.com/m5stack/M5Unified>
- M5GFX: <https://github.com/m5stack/M5GFX>
- LilyGo-EPD47: <https://github.com/Xinyuan-LilyGO/LilyGo-EPD47>
- AS7331: <https://github.com/RobTillaart/AS7331>
- ArduinoJson: <https://github.com/bblanchon/ArduinoJson>
- QRCode: <https://github.com/ricmoo/QRCode>
- arduinoWebSockets: <https://github.com/Links2004/arduinoWebSockets>
- SdFat: <https://github.com/greiman/SdFat>
- LibSSH-ESP32: <https://github.com/ewpa/LibSSH-ESP32>
- Expat: <https://github.com/libexpat/libexpat>
- miniz: <https://github.com/richgel999/miniz>
- picojpeg: <https://github.com/richgel999/picojpeg>
- Noto Fonts: <https://github.com/notofonts/latin-greek-cyrillic>
- OpenDyslexic: <https://github.com/antijingoist/open-dyslexic>
- Ubuntu Font Family: <https://github.com/canonical/Ubuntu-Sans-fonts>
- Arduino-ESP32 Core (WiFi/HTTP/BLE APIs used by apps/tools): <https://github.com/espressif/arduino-esp32>

App/component source projects:
- Poodle gameplay source reference: <https://github.com/k-natori/Poodle>
- Sudoku gameplay source reference: <https://gist.github.com/palaniraja/d5a55f9bd1f990410c8a0099844cec91>
- ESP32 Bus Pirate diagnostics source: <https://github.com/geo-tp/ESP32-Bus-Pirate>
- M5Cardputer Interactive SSH Client reference: <https://github.com/SUB0PT1MAL/M5Cardputer_Interactive_SSH_Client>
- M5Paper trackpad reference: <https://github.com/hollyhockberry/m5paper-trackpad>

## Function Catalog With Source Repositories

The launcher functions and where their code comes from:

| Function | Local implementation | Source repositories used |
|---|---|---|
| Reader (EPUB/TXT/XTC reading, progress, chapters, typography) | `src/activities/reader/*`, `lib/Epub/*`, `lib/Txt/*`, `lib/Xtc/*` | CrossPoint-reader, Expat, miniz, picojpeg, SdFat |
| Library (`Recent` / `Files`, cover preview panel) | `src/activities/home/*` | CrossPoint-reader + this fork enhancements |
| EPUB inline image rendering (JPEG/BMP) | `lib/Epub/Epub/Page.*`, `lib/Epub/Epub/Section.cpp`, `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.*` | CrossPoint-reader + this fork enhancements, miniz, picojpeg |
| Dashboard | `src/activities/apps/DashboardActivity.*` | CrossPoint-reader + ArduinoJson |
| Sensors: Built-in / External scan | `src/activities/apps/SensorsActivity.*` | CrossPoint-reader, M5Unified |
| Sensors: I2C Tools | `src/activities/apps/I2CToolsActivity.*` | ESP32-Bus-Pirate (source project), CrossPoint-reader, M5Unified |
| Sensors: UV (AS7331) | `src/activities/apps/UvSensorActivity.*` | AS7331 library, CrossPoint-reader, M5Unified |
| Sensors: UV Logs | `src/activities/apps/UvLogViewerActivity.*` | CrossPoint-reader + this fork additions |
| Sensors: GPIO Monitor | `src/activities/apps/GpioMonitorActivity.*` | ESP32-Bus-Pirate (source project), CrossPoint-reader |
| Sensors: UART RX Monitor | `src/activities/apps/UartRxMonitorActivity.*` | ESP32-Bus-Pirate (source project), CrossPoint-reader |
| Sensors: EEPROM Dump | `src/activities/apps/I2cEepromDumpActivity.*` | ESP32-Bus-Pirate (source project), CrossPoint-reader |
| Weather (internet retrieval + parse) | `src/activities/apps/WeatherActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core, ArduinoJson |
| Network: WiFi Scan | `src/activities/network/WifiScannerActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Network: WiFi Status | `src/activities/network/WifiStatusActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Network: WiFi Tests | `src/activities/network/WifiTestsActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Network: Channel Monitor | `src/activities/network/WifiChannelMonitorActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Network: BLE Scan | `src/activities/network/BleScannerActivity.*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Network: Web Portal | `src/activities/network/CrossPointWebServerActivity.*`, `src/network/CrossPointWebServer.*` | CrossPoint-reader + this fork additions, with workflow references including paperportal/portal |
| Network: Host Keyboard | `src/activities/apps/KeyboardHostActivity.*` | this fork implementation, with reference workflow from M5Paper_Keyboard and M5Paper_FactoryTest; uses Arduino-ESP32 BLE HID APIs |
| Network: Trackpad | `src/activities/apps/TrackpadActivity.*` | gesture logic adapted from m5paper-trackpad (MIT) + this fork integration using Arduino-ESP32 BLE HID APIs |
| Network: SSH Client | `src/activities/apps/SshClientActivity.*`, `lib/LibSSH-ESP32/*` | SSH session flow adapted from M5Cardputer_Interactive_SSH_Client; SSH transport from LibSSH-ESP32 |
| Games: Poodle | `src/activities/apps/PoodleActivity.*` | Gameplay source reference: k-natori/Poodle; implementation in this repository |
| Games: Sudoku | `src/activities/apps/SudokuActivity.*` | Gameplay source reference: palaniraja Sudoku gist; implementation in this repository |
| Images: Viewer | `src/activities/apps/ImageViewerActivity.*` | CrossPoint-reader + this fork additions, M5GFX |
| Images: Drawing app | `src/activities/apps/DrawingActivity.*` | CrossPoint-reader + this fork additions, M5GFX |
| Tools: Notes (with keyboard) | `src/activities/apps/NotesActivity.*`, `src/activities/util/KeyboardEntryActivity.*` | CrossPoint-reader + this fork additions |
| Tools: File Manager | `src/activities/apps/FileManagerActivity.*` | CrossPoint-reader + this fork additions, SdFat |
| Tools: Time | `src/activities/apps/TimeSettingsActivity.*` | CrossPoint-reader + this fork additions, M5Unified |
| Tools: Sleep Timer | `src/activities/apps/SleepTimerActivity.*` | CrossPoint-reader + this fork additions |
| Tools: OTA Update | `src/activities/apps/ToolsOtaUpdateActivity.*`, `src/network/OtaUpdater.*` | CrossPoint-reader, Arduino-ESP32 Core |
| Settings: WiFi | `src/activities/settings/CalibreSettingsActivity.*`, `src/activities/network/*` | CrossPoint-reader + this fork additions, Arduino-ESP32 Core |
| Settings: Hardware Test | `src/activities/apps/HardwareTestActivity.*` | CrossPoint-reader + this fork additions, M5Unified |
| Calculator | `src/activities/apps/CalculatorActivity.*` | this fork implementation on CrossPoint-reader framework |
| Web file server / upload / websocket transfer / idle hotspot web UI | `src/network/CrossPointWebServer.*`, `src/network/html/*`, `src/main.cpp` | CrossPoint-reader, arduinoWebSockets, ArduinoJson, QRCode |
| SD font loading and custom fonts | `src/fonts/*`, `src/CrossPointSettings.*` | CrossPoint-reader + this fork additions, M5GFX, Noto/OpenDyslexic/Ubuntu font projects |

## Additional M5Paper References Reviewed

The following M5Paper ecosystem repositories were reviewed during feature planning and UI integration.  
Where direct code adaptation occurred, it is explicitly recorded in `CODE_PROVENANCE.md`.

- M5PaperUI: <https://github.com/grundprinzip/M5PaperUI>
- M5Paper_FactoryTest: <https://github.com/m5stack/M5Paper_FactoryTest>
- M5PaperWeather: <https://github.com/Bastelschlumpf/M5PaperWeather>
- m5panel: <https://github.com/TAKeanice/m5panel>
- m5paper-dashboard: <https://github.com/estshorter/m5paper-dashboard>
- M5Paper_Keyboard: <https://github.com/robo8080/M5Paper_Keyboard>
- M5Paper_Remote_Dashboard: <https://github.com/capi/M5Paper_Remote_Dashboard>
- M5Sudoku: <https://github.com/murraypaul/M5Sudoku>
- M5PEasyTag: <https://github.com/nananauno/M5PEasyTag>
- M5Paper-ePub-reader: <https://github.com/Lefucjusz/M5Paper-ePub-reader>
- m5paper-sos: <https://github.com/inajob/m5paper-sos>
- m5paper-trackpad: <https://github.com/hollyhockberry/m5paper-trackpad>
- portal: <https://github.com/paperportal/portal>

## License

Project license: MIT (`LICENSE`).
