# Changelog

All notable changes to OmniPaper are documented in this file.

The release tag must match the version in `platformio.ini` exactly, without a leading `v`, so OTA version checks and release asset names stay aligned.

## [0.18.0] - 2026-03-13

### Added

- PaperS3 portrait-first launcher, settings, time, dashboard, sensors, and hardware-test layouts with larger title/footer typography and large rectangular tap targets.
- Direct-tap PaperS3 navigation for launcher tiles, submenu rows, and settings lists so the UI responds to the point touched instead of forcing swipe-to-select behavior.
- PaperS3 USB diagnostics that distinguish USB cable presence (`VBUS`) from an open CDC serial session, making OTG status readable on-device.
- PaperS3 on-screen keyboard integration for Poodle, notes, and Wi-Fi password entry so text-driven apps remain usable without external input hardware.
- A PaperS3 scientific calculator layout with trigonometric, logarithmic, square, reciprocal, and square-root functions.
- Tetris as an additional built-in game for the PaperS3 release.

### Changed

- Tuned the PaperS3 UI around the actual 960x540 panel, using a 540x960 logical portrait layout instead of the earlier landscape-oriented presentation.
- Switched the PaperS3 firmware target into TinyUSB USB-OTG mode instead of hardware CDC/JTAG mode so OTG-capable device features are available in the firmware configuration.
- Enlarged PaperS3 footer/status fonts, back chips, primary action buttons, and touch hit zones to improve usability on-device.
- Moved `Trackpad` out of the Network section and into Tools, where it belongs in the PaperS3 information architecture.
- Replaced generic PaperS3 submenu rows with square action badges and removed redundant row-level `Open` labels.
- Narrowed the tagged release bundle to the `m5papers3` firmware family so the published release matches the board that is now the primary supported target.

### Fixed

- Removed swipe-to-select navigation from the PaperS3 touch path, which was the main cause of the launcher feeling indirect and unresponsive on real hardware.
- Corrected PaperS3 USB presence detection so a plugged cable is no longer reported as disconnected just because no serial terminal has opened the CDC interface yet.
- Fixed multiple PaperS3 migration regressions in launcher/settings helper geometry, include paths, and shared font/header wiring uncovered during compile verification.
- Stabilized the PaperS3 reader and notes flows by removing separate PaperS3 render tasks from keyboard and library screens, avoiding UI-task races that could reboot the device.
- Guarded PaperS3 touch telemetry calls in dashboard, sensors, hardware test, and trackpad paths so unavailable touch state no longer causes crashy diagnostics screens.
- Fixed Weather, Image Viewer, and Wi-Fi selection behavior on PaperS3 by improving busy-state rendering, validating bitmap headers before draw, and hard-resetting Wi-Fi scan state before async scans.
- Increased PaperS3 Wi-Fi scan reliability with explicit `WIFI_STA` reinitialization, power-save disablement, scan cache clearing, and direct-tap network selection.
- Cleaned vendored LibSSH warning noise in the release build by fixing const-incompatible compatibility strings and properly scoping deprecated-SCP warning suppression.

## [0.17.0] - 2026-03-13

### Added

- M5PaperS3-first firmware support, including the custom `m5stack_papers3` board definition, updated PaperS3 SD wiring, PaperS3 input handling, and PaperS3-specific sleep handling.
- PaperS3-optimized diagnostics and dashboard views that surface USB/OTG session state, VBUS, battery current, RTC state, touch-point telemetry, IMU availability, and a speaker chirp hardware test.
- Repository compliance infrastructure: SPDX SBOM generation, release compliance checks, contributor/source-credit matrices, and a documented release runbook.
- Release metadata assets, including this changelog, generated release notes, build metadata, and SHA256 checksums in tagged firmware bundles.

### Changed

- Switched the default PlatformIO target to `m5papers3` so PaperS3 is the primary build and verification path.
- Reworked several status/diagnostic screens into touch-first card layouts tuned for the PaperS3 display.
- Standardized release artifact naming around board-specific OmniPaper firmware bundles for `m5papers3` and `m5paper`.

### Fixed

- Corrected stale CrossPoint OTA release references so firmware updates track OmniPaper GitHub releases instead of the legacy upstream repository.
- Fixed release/compliance drift by bundling the required notices, provenance files, contributor credits, and SBOM output with tagged releases.
- Removed first-party personal identifiers and funding metadata from repository-owned project files while preserving required third-party author attributions and notices.
