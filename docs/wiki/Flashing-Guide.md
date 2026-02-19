# Flashing Guide

## Firmware Files

Get prebuilt binaries here:
- https://github.com/Einnovoeg/OmniPaper/releases/latest

For each board, use the matching `*-firmware.bin`:
- `omnipaper-v0.17.0-m5paper-firmware.bin`
- `omnipaper-v0.17.0-m5papers3-firmware.bin`
- `omnipaper-v0.17.0-lilygo-epd47-firmware.bin`

## Recommended (PlatformIO)
Use your board environment:

```bash
pio run -e m5paper_release --target upload --upload-port <PORT>
pio run -e m5papers3 --target upload --upload-port <PORT>
pio run -e lilygo_epd47 --target upload --upload-port <PORT>
```

Install PlatformIO:
- https://docs.platformio.org/en/latest/core/installation/
- https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html

## Web-Based Flashing (No Local Install)

### Option A: Espressif ESP Tool (esptool-js)
- Web app: https://espressif.github.io/esptool-js/
- Docs: https://espressif.github.io/esptool-js/docs/index.html

Steps:
1. Open the web app in Chrome or Edge (Web Serial required).
2. Connect the device by USB and click `Connect`.
3. Add your board firmware file at flash address `0x10000`.
4. Click `Program`.
5. Reboot the board after flashing.

### Option B: ESP Launchpad (Browser Flasher)
- https://espressif.github.io/esp-launchpad/

Use `DIY` mode to flash local `.bin` files.

## Downloadable / Installed Flashing Tools

### Cross-platform: esptool (macOS / Linux / Windows)
- Install docs: https://docs.espressif.com/projects/esptool/en/latest/installation.html
- Binary releases: https://github.com/espressif/esptool/releases

#### Install with Python
```bash
python -m pip install --upgrade esptool
```

#### Flash firmware-only image

macOS/Linux:
```bash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-XXXX write_flash 0x10000 omnipaper-v0.17.0-m5paper-firmware.bin
```

Windows:
```powershell
py -m esptool --chip esp32 --port COM5 write_flash 0x10000 omnipaper-v0.17.0-m5paper-firmware.bin
```

Adjust `--chip` and firmware filename for your target:
- `esp32`: M5Paper
- `esp32s3`: M5PaperS3 and LilyGo-EPD47

### Windows GUI: Espressif Flash Download Tool
- Download page: https://www.espressif.com/en/tools-type/flash-download-tools

Steps:
1. Open Flash Download Tool.
2. Select your chip family.
3. Load firmware file and set address `0x10000`.
4. Select COM port and baud.
5. Click `START`.

## Flash release firmware binaries directly
All firmware binaries are flashed at `0x10000` in this project.

### M5Paper
```bash
python3 -m esptool --chip esp32 --port <PORT> write_flash 0x10000 omnipaper-v0.17.0-m5paper-firmware.bin
```

### M5PaperS3
```bash
python3 -m esptool --chip esp32s3 --port <PORT> write_flash 0x10000 omnipaper-v0.17.0-m5papers3-firmware.bin
```

### LilyGo-EPD47
```bash
python3 -m esptool --chip esp32s3 --port <PORT> write_flash 0x10000 omnipaper-v0.17.0-lilygo-epd47-firmware.bin
```

## Optional full-flash recovery
Use each board's release `bootloader.bin` + `partitions.bin` + `firmware.bin` when full recovery is required.

Typical offsets used in this project:
- `0x1000` bootloader
- `0x8000` partitions
- `0x10000` firmware
