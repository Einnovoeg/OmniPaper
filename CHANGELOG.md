# Changelog

All notable changes to OmniPaper are documented in this file.

The release tag must match the version in `platformio.ini` exactly, without a leading `v`, so OTA version checks and release asset names stay aligned.

## [0.17.0] - 2026-03-13

### Added

- M5PaperS3-first firmware support, including the custom `m5stack_papers3` board definition, updated PaperS3 SD wiring, PaperS3 input handling, and PaperS3-specific sleep handling.
- PaperS3-optimized diagnostics and dashboard views that surface USB/OTG session state, VBUS, battery current, RTC state, touch-point telemetry, IMU availability, and a speaker chirp hardware test.
- Repository compliance infrastructure: SPDX SBOM generation, release compliance checks, contributor/source-credit matrices, and a documented release runbook.
- Release metadata assets, including this changelog, generated release notes, build metadata, and SHA256 checksums in tagged firmware bundles.

### Changed

- Switched the default PlatformIO target to `m5papers3` so PaperS3 is the primary build and verification path.
- Reworked several status/diagnostic screens into touch-first card layouts tuned for the PaperS3 800x480 experience.
- Standardized release artifact naming around board-specific OmniPaper firmware bundles for `m5papers3` and `m5paper`.

### Fixed

- Corrected stale CrossPoint OTA release references so firmware updates track OmniPaper GitHub releases instead of the legacy upstream repository.
- Fixed release/compliance drift by bundling the required notices, provenance files, contributor credits, and SBOM output with tagged releases.
- Removed first-party personal identifiers and funding metadata from repository-owned project files while preserving required third-party author attributions and notices.
