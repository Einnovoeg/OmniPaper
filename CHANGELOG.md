# Changelog

All notable changes to OmniPaper are documented in this file.

The release tag must match the version in `platformio.ini` exactly, without a leading `v`, so OTA version checks and release asset names stay aligned.

## [0.18.4] - 2026-03-15

### Changed

- Replaced the generic PaperS3 launcher overlay with a dedicated top status strip so battery, Wi-Fi, and time no longer fight the PaperS3 header/back-button area.
- Lowered the shared PaperS3 header/back/footer geometry again to keep titles, navigation chips, and footer text inside the visible safe area on-device.
- Rebuilt the PaperS3 drawing screen as a portrait-first canvas with batched touch stroke flushing instead of the older landscape/raw-touch implementation.
- Trimmed the default PaperS3 dashboard, sensors, and hardware-test overview probes down to the safest always-on telemetry while keeping deeper diagnostics available in dedicated screens.

### Fixed

- Implemented diagonal line drawing in the shared renderer, which restores PaperS3 launcher icons, submenu arrows, and other diagonal UI glyphs that were previously missing.
- Realigned the PaperS3 virtual Back touch zone with the visible Back chip after the shared header moved downward.
- Fixed the PaperS3 Wi-Fi save/forget prompts so touch selection now actually writes or removes SD-backed credentials instead of behaving like a button-only dialog.
- Reduced PaperS3 touch drawing latency by batching canvas updates during a stroke instead of refreshing the panel after every single touch sample.
- Removed several high-risk live PMIC/touch/IMU overview reads from the PaperS3 dashboard and diagnostics screens to reduce the crash/reset reports coming from those entry points.

## [0.18.3] - 2026-03-14

### Changed

- Shifted the PaperS3 portrait chrome further down the page and enlarged the overlay font so the top status bar, back chip, and screen titles stay inside the visible safe area instead of colliding at the top edge.
- Cleaned up the PaperS3 launcher copy by removing redundant tap-instruction text and turning the Reader tile into a direct entry point to the touch-first file browser on PaperS3.
- Removed the extra `APP` / `MENU` pills from PaperS3 launcher tiles so the front page is less cluttered and icon/title alignment has more room.
- Reinitialized the Wi-Fi radio more conservatively before PaperS3 scans by cycling through `WIFI_OFF` and back to `WIFI_STA`, which reduces empty scan results after other network modes have been used.
- Moved the PaperS3 Settings, Time Settings, and Hardware Test screens onto the shared header safe area instead of bespoke top-of-screen title coordinates.

### Fixed

- Made PaperS3 touch-first screens react on touch begin instead of waiting for a short release gesture, which substantially improves responsiveness on the slow-refresh e-paper panel and makes launcher/game taps feel immediate.
- Fixed the PaperS3 launcher icon rendering bug that was drawing icons in the same color as their own icon boxes, which is why the icons looked missing on-device.
- Updated the PaperS3 virtual Back touch zone to match the moved header chip so touch-generated navigation lines up with the visible UI again.
- Removed the remaining PaperS3 background render tasks from the EPUB/XTC reader submenus and chapter selectors, extending the same race-condition fix already applied to the main reader screens.
- Removed the separate PaperS3 render task from the settings category browser and top-level settings screen, closing another task-vs-touch race that could destabilize touch navigation on-device.
- Increased PaperS3 footer status typography and lowered the compact overlay line again so the top and bottom status text remain readable on the higher-resolution panel.

## [0.18.2] - 2026-03-14

### Changed

- Reworked the shared PaperS3 header, list, and overlay spacing so the date/time overlay, back button, titles, and launcher rows no longer collide at the top of the portrait UI.
- Switched tagged releases to a single merged `omnipaper-<tag>-m5papers3.bin` image instead of publishing a bundle of text files and split flash segments.
- Made the Poodle PaperS3 layout denser and more explicit by moving the keyboard higher on-screen and labeling the keyboard block directly.

### Fixed

- Removed the remaining PaperS3 background render tasks from the EPUB, TXT, and XTC readers, which were still causing touch/render races and reader resets on-device.
- Stabilized PaperS3 Wi-Fi network selection by moving its scan/render flow onto the main loop and using a synchronous scan path tuned for the S3 instead of the older async path.
- Applied the same synchronous PaperS3 scan fix to the standalone Wi-Fi scanner so nearby networks now populate reliably there as well.
- Stopped Weather from auto-fetching immediately on entry and fixed its failure-state redraw path, preventing the app from feeling hung or crashing as soon as it opens.
- Fixed the Dashboard weather refresh path so it paints a loading state before blocking network work and redraws properly after failures.
- Replaced PaperS3 submenu text badges with actual icons and fixed selected-state icon rendering so launcher rows and highlighted tiles remain legible.

## [0.18.1] - 2026-03-14

### Added

- A native in-app Poodle keyboard for PaperS3 so the word game is directly playable by touch without bouncing into the generic text-entry activity.
- Per-key feedback in Poodle for hit/present/miss states, plus dictionary-backed guess validation and a persistent PaperS3 status line.
- Metadata chips and clearer touch hints on PaperS3 launcher tiles and submenu rows, informed by `bmorcelli/Launcher` and adapted locally for OmniPaper.

### Changed

- Increased PaperS3 tap recognition tolerance in the shared touch adapter so slightly slower or heavier e-paper taps still register as intended.
- Enlarged shared PaperS3 header, list, action-button, footer, and status typography to better match the 540x960 portrait layout on-device.
- Refined the PaperS3 launcher tile composition to emphasize direct touch, with denser descriptions and square-card metadata instead of bare placeholder cards.

### Fixed

- Replaced the ineffective PaperS3 Poodle interaction model with direct cell selection, integrated touch keyboard entry, clear/delete actions, and proper submit flow.
- Fixed Poodle duplicate-letter scoring so repeated letters now follow the expected two-pass word-match logic instead of naive single-character checks.
- Documented the new launcher reference source in provenance and third-party notices so the PaperS3 UX polish remains license-compliant and fully attributed.

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
