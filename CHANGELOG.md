# Changelog

## [Unreleased]

## 0.2.0 - 2026-08-01

### Added

- Persistent 32-bit layout options and per-layer encoder maps through commands
  `0x02/0x02`, `0x03/0x02`, `0x14`, and `0x15`.
- Generic custom-value save routing and explicit custom-state size validation.
- RP2040 Adafruit TinyUSB Raw HID and boot-keyboard adapters, plus an EEPROM
  storage adapter; these adapters are compile-tested, not hardware-certified.
- Response retrying so a request is not consumed again after a failed send.

### Changed

- `Callbacks::matrixRow(uint8_t)` now returns `uint32_t`; matrix responses pack
  1-4 bytes per row in big-endian order for matrices up to 32 columns.
- `Callbacks::deviceIndication(uint8_t)` receives the full protocol value.
  Existing `deviceIndication(bool)` overrides remain usable through the default
  forwarding overload, but should migrate when values other than 0/1 matter.
- `Config` appends layout, encoder, storage-workspace, and security fields.
  Existing positional initializers remain valid because all new fields have
  defaults; new code should assign optional fields explicitly for readability.
- Keymap reset (`0x06`) also restores default encoder maps but no longer clears
  macros. Macro reset (`0x10`) clears only macros.
- Persistence validates record size and CRC before changing active state.

### Security

- Matrix-state disclosure now requires `Config::matrixStateEnabled = true`.
- EEPROM factory reset now requires `Config::eepromResetEnabled = true`.
- Bootloader jump now requires `Config::bootloaderEnabled = true`, and its
  callback runs only after `task()` successfully sends the response.

### Migration

- Storage-backed configurations must provide a separate `Config::loadBuffer`
  of at least `Protocol::requiredLoadBufferSize()` bytes. `begin()` and `load()`
  reject missing, undersized, or overlapping workspaces before storage access.
- Set each security flag deliberately if upgrading code relied on matrix
  disclosure, factory reset, or bootloader jump being available.
- Applications remain responsible for matrix scanning and executing returned
  QMK/VIA keycodes as HID reports.

## 0.1.0 - 2026-07-26

- Initial portable VIA protocol core.
- VIA protocol v13 commands for dynamic keymaps, macro buffer, layers,
  keyboard values, persistence, and custom values.
- RGBLight custom-value channel and in-memory reference adapters.
- Arduino library metadata, example, tests, documentation, and CI.
