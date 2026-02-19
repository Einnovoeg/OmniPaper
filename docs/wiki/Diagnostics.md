# Diagnostics

## IT8951 Probe (M5Paper)
Diagnostic image:
- `omnipaper-v0.17.0-m5paper-epd-diag-firmware.bin`

Purpose:
- minimal diagnostic firmware that probes IT8951 EPD communication and logs pass/fail over serial.

Flash:
```bash
python3 -m esptool --chip esp32 --port <PORT> write_flash 0x10000 omnipaper-v0.17.0-m5paper-epd-diag-firmware.bin
```
