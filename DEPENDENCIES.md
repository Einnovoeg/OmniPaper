# Dependencies

This project is built with PlatformIO. Most dependencies are resolved automatically from `platformio.ini`.

## Required Tooling

- PlatformIO Core 6.x (`pio` CLI)
- Python 3.10+
- Git
- `scripts/compliance.py` for repository/release compliance checks and SPDX SBOM generation

## Framework / Platform

- `platform = espressif32 @ 6.12.0`
- `framework = arduino`

## Direct Library Dependencies

Defined in `platformio.ini`:

- `https://github.com/m5stack/M5Unified.git`
- `https://github.com/m5stack/M5GFX.git`
- `https://github.com/RobTillaart/AS7331.git`
- `bblanchon/ArduinoJson @ 7.4.2`
- `ricmoo/QRCode @ 0.0.1`
- `links2004/WebSockets @ 2.7.3`
- `WiFi` (Arduino-ESP32 framework library)

Local symlinked libraries:

- `open-x4-sdk/libs/display/M5PaperDisplayAdapter`
- `open-x4-sdk/libs/hardware/M5PaperInputAdapter`
- `open-x4-sdk/libs/hardware/SDCardManager`

## Transitive / Included Components

Resolved through PlatformIO and local vendored libraries, including:

- `SdFat`
- `ESPmDNS`, `DNSServer`, `WebServer`, `HTTPClient`, `WiFiClientSecure`
- `ESP32 BLE Arduino`
- Vendored in `lib/`: `LibSSH-ESP32`, `expat`, `miniz`, `picojpeg`, and project modules

## Optional Runtime Requirements

- microSD card (recommended for books/assets/fonts/logs)
- Wi-Fi network for weather, OTA, OPDS, SSH, and web portal features

## Release Build Environments

- `m5papers3_release`
- `m5paper_release`
