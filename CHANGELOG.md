# Changelog

## [Unreleased]

## 0.2.0 - 2026-08-01

### Added

- Persistent 32-bit layout options and per-layer encoder maps through commands
  `0x02/0x02`, `0x03/0x02`, `0x14`, and `0x15`.
- Generic custom-value save routing and explicit custom-state size validation.
- `CustomValue::validateState()` provides non-mutating validation before staged
  custom state is published or factory-reset storage is changed.
- RP2040 Adafruit TinyUSB Raw HID and boot-keyboard adapters, plus an EEPROM
  storage adapter; these adapters are compile-tested, not hardware-certified.
- Response retrying so a request is not consumed again after a failed send.
- `Transport::sendComplete()` lets asynchronous adapters report when an accepted
  response transfer has completed; synchronous adapters inherit immediate
  completion.

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
- Persistence reads each record payload once into caller staging, validates and
  publishes those same bytes, clears dirty state on success, and preserves it on
  failure.
- RP2040 Raw HID releases its single callback owner after failed initialization
  or owner destruction so a replacement instance can initialize.

### Security

- Matrix-state disclosure now requires `Config::matrixStateEnabled = true`.
- EEPROM factory reset now requires `Config::eepromResetEnabled = true`.
- Bootloader jump requires `Config::bootloaderEnabled = true`, a callback, and a
  successful dirty-state save. Its callback runs only after the transport
  reports the accepted response transfer complete.

### Migration

- The persistent record schema changed from version 1 to version 2. Existing
  0.1.0 records are rejected. On the first 0.2.0 `begin()`, the rejected load
  triggers the normal one-time startup fallback: the configured default keymap,
  encoder maps, and layout options are restored, and macros are cleared. No
  automatic migration or save exists; users must reconfigure in VIA and save a
  new record.
- Storage-backed configurations must provide a separate `Config::loadBuffer`
  of at least `Protocol::requiredLoadBufferSize()` bytes. The core rejects a
  missing or undersized workspace and overlap with configured keymap, encoder-
  map, or macro buffers. It cannot inspect `CustomValue` handler-owned active
  state; callers must keep that state separate as documented in `PORTING.md`.
- Set each security flag deliberately if upgrading code relied on matrix
  disclosure, factory reset, or bootloader jump being available.
- Custom-value handlers that can reject persisted bytes must move that decision
  into a `const`, non-mutating `validateState()` override. Returning `true`
  guarantees the following `loadState()` for those bytes succeeds and publishes
  them; returning `false` from `loadState()` must still leave active state
  unchanged. Handlers needing only exact `stateSize()` validation can use the
  default implementation unchanged.
- Applications remain responsible for matrix scanning and executing returned
  QMK/VIA keycodes as HID reports.

## 0.1.0 - 2026-07-26

- Initial portable VIA protocol core.
- VIA protocol v13 commands for dynamic keymaps, macro buffer, layers,
  keyboard values, persistence, and custom values.
- RGBLight custom-value channel and in-memory reference adapters.
- Arduino library metadata, example, tests, documentation, and CI.
