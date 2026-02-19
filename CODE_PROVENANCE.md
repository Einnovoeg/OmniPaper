# CODE_PROVENANCE

This document records where code in **OmniPaper** comes from and how each source is used.

## 1. CrossPoint-reader

### 1(a) Upstream repository

- Upstream project: [CrossPoint-reader](https://github.com/daveallie/crosspoint-reader)
- This repository is a fork/adaptation of that codebase for OmniPaper board targets.

### 1(b) Specific enhancements in this fork

Enhancements added in this fork include:

- Multi-target PlatformIO build matrix
  - `platformio.ini`
- LilyGo board definition sourced from LilyGo-EPD47 project metadata
  - `boards/lilygo_t5_47_plus_s3.json`
- Boot icon-grid launcher and app/category menu system
  - `src/activities/launcher/LauncherActivity.cpp`
  - `src/activities/launcher/LauncherActions.h`
  - `src/main.cpp`
- EPUB picker cover-preview panel
  - `src/activities/home/MyLibraryActivity.cpp`
  - `src/activities/home/MyLibraryActivity.h`
- Inline EPUB image rendering (JPEG/BMP)
  - `lib/Epub/Epub/Page.h`
  - `lib/Epub/Epub/Page.cpp`
  - `lib/Epub/Epub/Section.cpp`
  - `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`
  - `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`
  - `src/activities/reader/EpubReaderActivity.cpp`
- User-provided SD font families (`/fonts/user_*.epf`) and reader font option
  - `src/fontIds.h`
  - `src/CrossPointSettings.h`
  - `src/CrossPointSettings.cpp`
  - `src/fonts/SdFontLoader.cpp`
  - `src/activities/settings/SettingsActivity.cpp`
- UTF-8 SD long-file-name support in build flags
  - `platformio.ini`
- Added apps/network tools for M5Paper
  - `src/activities/apps/*`
  - `src/activities/network/*`

## 2. Third-Party Code Included in This Firmware

| Component | Upstream URL | License | How code is used here | Local path / integration |
|---|---|---|---|---|
| CrossPoint-reader | https://github.com/daveallie/crosspoint-reader | MIT | Fork base code | repository baseline |
| open-x4 community SDK | https://github.com/open-x4-epaper/community-sdk | MIT | Git submodule code | `open-x4-sdk/` |
| LilyGo-EPD47 | https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 | MIT | Board-definition metadata for PlatformIO target setup | `boards/lilygo_t5_47_plus_s3.json` |
| EPDiy | https://github.com/vroland/epdiy | LGPL-3.0 (upstream) | Source provenance for EPD font data structure and font conversion script | `lib/EpdFont/EpdFontData.h`, `lib/EpdFont/scripts/fontconvert.py` |
| Expat | https://github.com/libexpat/libexpat | MIT | Vendored source code | `lib/expat/` |
| miniz | https://github.com/richgel999/miniz | MIT | Vendored source code | `lib/miniz/` |
| picojpeg | https://github.com/richgel999/picojpeg | Public Domain | Vendored source code | `lib/picojpeg/` |
| M5Unified | https://github.com/m5stack/M5Unified | MIT | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| M5GFX | https://github.com/m5stack/M5GFX | MIT | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| Arduino-ESP32 Core | https://github.com/espressif/arduino-esp32 | LGPL-2.1-or-later | Framework source code used at build/runtime (WiFi/HTTP/BLE/Update APIs) | PlatformIO framework package |
| AS7331 | https://github.com/RobTillaart/AS7331 | MIT | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| ArduinoJson | https://github.com/bblanchon/ArduinoJson | MIT | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| QRCode | https://github.com/ricmoo/QRCode | MIT | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| arduinoWebSockets | https://github.com/Links2004/arduinoWebSockets | LGPL-2.1 | Dependency-managed source code at build time | `platformio.ini` (`lib_deps`) |
| SdFat | https://github.com/greiman/SdFat | MIT | Dependency-managed source code at build time | transitive dependency |
| LibSSH-ESP32 | https://github.com/ewpa/LibSSH-ESP32 | LGPL-2.1 (with BSD components) | Vendored source code for SSH client transport layer | `lib/LibSSH-ESP32/` |

## 3. Additional Components (Menu, Apps, Games)

### 3(a) Menu system

| Component | Upstream URL | How code is sourced |
|---|---|---|
| Activity/menu framework | https://github.com/daveallie/crosspoint-reader | Fork base framework used and extended |
| M5 icon-grid launcher implementation | https://github.com/daveallie/crosspoint-reader | Local implementation in fork using upstream activity model |

Files:
- `src/activities/launcher/LauncherActivity.cpp`
- `src/activities/launcher/LauncherActivity.h`
- `src/activities/launcher/LauncherActions.h`
- `src/main.cpp`

### 3(b) Added apps, games, and code

| Component | Source URL | License status | How code is sourced in this repository | Local files |
|---|---|---|---|---|
| Poodle game | https://github.com/k-natori/Poodle | No repository-wide code license declared upstream | Gameplay/rules referenced; implementation is local in this repository and does not copy upstream source files | `src/activities/apps/PoodleActivity.cpp` |
| Sudoku game | https://gist.github.com/palaniraja/d5a55f9bd1f990410c8a0099844cec91 | No explicit license shown on gist page | Puzzle/gameplay reference used; implementation is local in this repository and does not copy source from the gist | `src/activities/apps/SudokuActivity.cpp` |
| I2C/GPIO/UART/EEPROM diagnostics | https://github.com/geo-tp/ESP32-Bus-Pirate | MIT | Feature workflow and tool scope sourced from upstream project; implementation adapted locally for M5Paper | `src/activities/apps/I2CToolsActivity.cpp`, `src/activities/apps/GpioMonitorActivity.cpp`, `src/activities/apps/UartRxMonitorActivity.cpp`, `src/activities/apps/I2cEepromDumpActivity.cpp` |
| UV sensor app | https://github.com/RobTillaart/AS7331 | MIT | Uses upstream library API directly via dependency | `src/activities/apps/UvSensorActivity.cpp` |
| Notes app + keyboard | https://github.com/daveallie/crosspoint-reader | MIT | Built on forked CrossPoint-reader activity framework; implementation local in this repository | `src/activities/apps/NotesActivity.cpp`, `src/activities/util/KeyboardEntryActivity.cpp` |
| Drawing app | https://github.com/daveallie/crosspoint-reader | MIT | Built on forked CrossPoint-reader activity framework; implementation local in this repository | `src/activities/apps/DrawingActivity.cpp` |
| Image viewer | https://github.com/daveallie/crosspoint-reader | MIT | Built on forked CrossPoint-reader activity framework; implementation local in this repository | `src/activities/apps/ImageViewerActivity.cpp` |
| Calculator | https://github.com/daveallie/crosspoint-reader | MIT | Built on forked framework; implementation local in this repository | `src/activities/apps/CalculatorActivity.cpp` |
| Wi-Fi/BLE network tools | https://github.com/daveallie/crosspoint-reader | MIT | Built on forked framework + ESP32 Arduino APIs; implementation local in this repository | `src/activities/network/WifiScannerActivity.cpp`, `src/activities/network/WifiStatusActivity.cpp`, `src/activities/network/WifiTestsActivity.cpp`, `src/activities/network/WifiChannelMonitorActivity.cpp`, `src/activities/network/BleScannerActivity.cpp` |
| Web portal launcher mode (manual STA/AP portal) | https://github.com/paperportal/portal and https://github.com/daveallie/crosspoint-reader | (No explicit license file in referenced portal repo) / MIT | Portal behavior/workflow reference plus local implementation in this repository; no source files copied from unlicensed portal repo | `src/activities/network/CrossPointWebServerActivity.cpp`, `src/network/CrossPointWebServer.cpp`, `src/main.cpp` |
| Host keyboard mode (BLE HID + USB serial fallback) | https://github.com/robo8080/M5Paper_Keyboard and https://github.com/m5stack/M5Paper_FactoryTest | MIT / MIT | BLE/serial keyboard workflow adapted to local launcher/activity framework; implementation local in this repository | `src/activities/apps/KeyboardHostActivity.cpp`, `src/activities/apps/KeyboardHostActivity.h` |
| Trackpad mode (BLE HID mouse) | https://github.com/hollyhockberry/m5paper-trackpad | MIT | Gesture event timing/classification adapted from upstream trackpad project, then integrated with local BLE HID mouse implementation | `src/activities/apps/TrackpadActivity.cpp`, `src/activities/apps/TrackpadActivity.h` |
| SSH client mode | https://github.com/SUB0PT1MAL/M5Cardputer_Interactive_SSH_Client and https://github.com/ewpa/LibSSH-ESP32 | (No explicit license file in referenced SSH UI repo) / LGPL-2.1 | SSH UI/interaction flow referenced from the app and implemented locally; no source files copied from unlicensed SSH UI repo. SSH transport implementation uses vendored LibSSH-ESP32 | `src/activities/apps/SshClientActivity.cpp`, `src/activities/apps/SshClientActivity.h`, `lib/LibSSH-ESP32/` |

### 3(c) Source citations summary

Primary sources used for this project:
- CrossPoint-reader: <https://github.com/daveallie/crosspoint-reader>
- open-x4 community SDK: <https://github.com/open-x4-epaper/community-sdk>
- LilyGo-EPD47: <https://github.com/Xinyuan-LilyGO/LilyGo-EPD47>
- EPDiy: <https://github.com/vroland/epdiy>
- M5Unified: <https://github.com/m5stack/M5Unified>
- M5GFX: <https://github.com/m5stack/M5GFX>
- AS7331: <https://github.com/RobTillaart/AS7331>
- ArduinoJson: <https://github.com/bblanchon/ArduinoJson>
- QRCode: <https://github.com/ricmoo/QRCode>
- arduinoWebSockets: <https://github.com/Links2004/arduinoWebSockets>
- SdFat: <https://github.com/greiman/SdFat>
- Expat: <https://github.com/libexpat/libexpat>
- miniz: <https://github.com/richgel999/miniz>
- picojpeg: <https://github.com/richgel999/picojpeg>
- Poodle: <https://github.com/k-natori/Poodle>
- Sudoku gist: <https://gist.github.com/palaniraja/d5a55f9bd1f990410c8a0099844cec91>
- ESP32 Bus Pirate: <https://github.com/geo-tp/ESP32-Bus-Pirate>
- M5Cardputer Interactive SSH Client: <https://github.com/SUB0PT1MAL/M5Cardputer_Interactive_SSH_Client>
- LibSSH-ESP32: <https://github.com/ewpa/LibSSH-ESP32>
- M5Paper_Keyboard: <https://github.com/robo8080/M5Paper_Keyboard>
- M5Paper_FactoryTest: <https://github.com/m5stack/M5Paper_FactoryTest>
- m5paper-trackpad: <https://github.com/hollyhockberry/m5paper-trackpad>
- M5PaperUI: <https://github.com/grundprinzip/M5PaperUI>
- M5PaperWeather: <https://github.com/Bastelschlumpf/M5PaperWeather>
- m5panel: <https://github.com/TAKeanice/m5panel>
- m5paper-dashboard: <https://github.com/estshorter/m5paper-dashboard>
- M5Paper_Remote_Dashboard: <https://github.com/capi/M5Paper_Remote_Dashboard>
- M5Sudoku: <https://github.com/murraypaul/M5Sudoku>
- M5PEasyTag: <https://github.com/nananauno/M5PEasyTag>
- M5Paper-ePub-reader: <https://github.com/Lefucjusz/M5Paper-ePub-reader>
- m5paper-sos: <https://github.com/inajob/m5paper-sos>
- portal: <https://github.com/paperportal/portal>
- XTC format info gist: <https://gist.github.com/CrazyCoder/b125f26d6987c0620058249f59f1327d>
- poly1305-donna (transitive reference via LibSSH-ESP32): <https://github.com/floodyberry/poly1305-donna>

For full license text and notices, see `THIRD_PARTY_NOTICES.md`.
