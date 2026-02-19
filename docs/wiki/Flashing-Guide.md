# Flashing Guide

## Recommended (PlatformIO)
Use your board environment:

```bash
pio run -e m5paper_release --target upload --upload-port <PORT>
pio run -e m5papers3 --target upload --upload-port <PORT>
pio run -e lilygo_epd47 --target upload --upload-port <PORT>
```

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
