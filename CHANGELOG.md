# Changelog

## [Unreleased]

## 0.4.0-experimental — 2026-08-02

### Added

- ESP32-S3 platform adapters: GPIO matrix IO, NVS storage, BLE keyboard HID,
  BLE GATT VIA transport (AirVIA).
- ESP32-S3 wireless sketch: dual-mode USB VIA config + BLE typing, compile-only.
- `VIA_Encoder` — gray-code quadrature state machine with configurable debounce.
- `VIA_Battery` — ADC voltage-to-percentage conversion with moving average.
- `VIA_SleepMgr` — idle timeout state machine for deep sleep coordination.

### Changed

- `library.properties` bumped to `0.4.0-experimental`.
- README expanded with full module listing, platform table, and all examples.
- CI adds `encoder`, `battery`, and `sleep` native test jobs with ASan/UBSan.

### Hardware

- ESP32-S3 adapters and wireless sketch compile in CI but are not
  hardware-verified. A custom PCB is required.
- ESP32-WROOM (classic) is not supported because it lacks a USB device
  peripheral.

## 0.3.0 — 2026-08-01

### Added

- `VIA_Matrix` with `COL2ROW`/`ROW2COL` scanning, 5 ms global symmetric
  deferred debounce, and changed-row detection.
- `VIA_Keyboard` with press/release event batching, transparent layer lookup
  with `MO`/`TG`/`TO`/`DF`, QMK 0.0.8 keycode classification, basic keyboard
  HID usages, physical modifiers, `QK_MODS`, and boot-protocol 6KRO reports.
- Busy-endpoint report retry, coalescing, host LED feedback, suspend/resume
  detection, and remote wake.
- STM32F103CB platform adapters: Cube USB composite device (keyboard + VIA
  Raw HID), dual-slot atomic flash storage, ROM USART bootloader coordinator,
  GPIO matrix IO.
- STM32F103 compile-only reference sketch.
- USB descriptor snapshot tests for boot keyboard and VIA Raw HID interfaces.

### Changed

- `Protocol` gains `rows()`, `columns()`, `layers()`, `keymap()` accessors
  for the keyboard engine.
- CI adds `matrix`, `keyboard`, `descriptor`, and `flash` native test jobs
  with ASan/UBSan; STM32 compile and size gate deferred until hardware
  toolchain is available.

### Hardware

- STM32F103 adapters and sketch compile in CI but are not hardware-verified.
  A custom PCB with genuine STM32F103CBT6 is required.

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
- RP2040 Raw HID keeps a successfully registered TinyUSB interface and callback
  storage alive for the device lifetime because TinyUSB has no unregister path.
  The adapter permits one begin attempt per device reset; a failed attempt
  registers nothing upstream but still rejects adapter retries and replacements.

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
  of at least `Protocol::requiredLoadBufferSize()` bytes.
- Custom-value handlers that can reject persisted bytes must move that decision
  into a `const`, non-mutating `validateState()` override.

## 0.1.0 - 2026-07-26

- Initial portable VIA protocol core.
- VIA protocol v13 commands for dynamic keymaps, macro buffer, layers,
  keyboard values, persistence, and custom values.
- RGBLight custom-value channel and in-memory reference adapters.
- Arduino library metadata, example, tests, documentation, and CI.
