# Changelog

## [Unreleased]
### Fixed
- **CRITICAL BUG FIX**: Re-wrote the `id_get_keyboard_value` (Command 0x02, Subcommand 0x03) matrix state byte-packing algorithm to fully conform to the QMK standard. The library can now dynamically pack 1 to 4 bytes per row based on `config_.columns`, supporting up to 32 columns.
- Changed `Callbacks::matrixRow` return type from `uint8_t` to `uint32_t` to support keyboards with more than 8 columns.

### Added
- Implemented `id_bootloader_jump` (Command 0x0B) with a new virtual `bootloaderJump()` function in `Callbacks`, allowing users to enter bootloader mode directly from the VIA UI.

## 0.1.0 — 2026-07-26

- Initial portable VIA protocol core.
- VIA protocol v13 commands for dynamic keymaps, macro buffer, layers,
  keyboard values, persistence, and custom values.
- RGBLight custom-value channel and in-memory reference adapters.
- Arduino library metadata, example, tests, documentation, and CI.
