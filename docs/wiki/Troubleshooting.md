# Troubleshooting

## No serial output
- Confirm correct serial port.
- Check baud rate (typically `115200`).
- Verify USB cable supports data.

## E-ink not refreshing
- Verify board type and build target match.
- Run the diagnostic build and capture serial logs.
- Confirm power stability during refresh.

## Flashing fails
- Ensure no other monitor app has locked the port.
- Use reset/boot procedure for your board if required.
