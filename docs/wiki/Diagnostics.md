# Diagnostics

## M5PaperS3 In-App Diagnostics

Use the regular firmware build for most PaperS3 checks:

- `Dashboard`
  - quick status board for RTC/time sync, Wi-Fi, weather, battery percentage, battery voltage/current, VBUS,
    USB serial session state, touch activity, IMU availability, and speaker availability
- `Settings -> Hardware Test`
  - live navigation/touch state
  - RTC summary
  - SD, SHT30, IMU checks
  - PaperS3 speaker chirp on confirm action
- `Sensors -> Built-In`
  - structured PaperS3 telemetry view for power, radio/system state, environment, motion, and touch
- `Sensors -> External`
  - external I2C scan for attached peripherals

## IT8951 Probe (M5Paper)
Diagnostic image:
- build locally with `pio run -e m5paper_epd_diag`

Purpose:
- minimal diagnostic firmware that probes IT8951 EPD communication and logs pass/fail over serial.

Flash:
```bash
python3 -m esptool --chip esp32 --port <PORT> write_flash 0x10000 .pio/build/m5paper_epd_diag/firmware.bin
```
