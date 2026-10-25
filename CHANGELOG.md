# Changelog

## [Unreleased]

### Added
- nRF52840 (nice!nano v2) platform adapters: Bluefruit BLE HID, AirVIA GATT transport, InternalFS (LittleFS) storage.
- `nice_nano_v2_VIA_BLE` example sketch and VIA JSON definition.
- Native tests and compile gates for nRF52 adapters.
- Generic ESP32 (WROOM-32/WROOM-32E) platform adapters: GPIO matrix IO,
  NVS shadow-blob storage, NimBLE 2.x BLE HID keyboard adapter, and AirVIA
  BLE GATT VIA transport (128-bit AirVIA UUIDs, 32-byte packets).
- `ESP32_WROOM32_VIA_BLE` example sketch: BLE-only wireless keyboard
  (5x6x3 matrix, rotary encoder) for ESP32-WROOM-32, which has no native
  USB.
- CI `esp32-adapters` job: native builds of the ESP32 adapter tests with
  `-Wall -Wextra -Werror` (ASan/UBSan for the BLE transport) plus
  arduino-cli compilation of the WROOM-32 reference sketch.

### Changed
- The AirVIA BLE transport for ESP32 targets the NimBLE-Arduino 2.x API:
  characteristic callbacks receive `NimBLEConnInfo`, and subscription state
  is tracked through `onSubscribe()` (NimBLE 1.x had no getter for it).
- ESP32 BLE adapters publish a standard 8-byte keyboard report map.
  The generic `via::esp32` GATT notification carries only the 8 report
  bytes; report ID 1 is supplied by the Report Reference descriptor.
- `via::esp32s3::BLEViaTransport` is now an alias for the generic
  `via::esp32::BLEViaTransport` implementation, so the S3 dual-mode sketch
  shares the NimBLE-Arduino 2.x AirVIA transport.
- `.gitignore` now excludes stray object files.
- ESP32 WROOM reference setup initializes `kColToRow` matrix column pull-ups and reports startup failures over serial.
- VIA protocol rejects out-of-range keycode reads and permits persistence commits without a custom-value handler.
- ESP32 BLE HID input reports expose readable report metadata alongside notifications.

### Fixed
- Protocol test expectation updated for the 0.3.x factory-reset semantics:
  without a `CustomValue::reset()` override a successful factory reset
  preserves the active custom value (no distinct default exists to restore).
- ESP32 BLE HID `send()` no longer truncates the 6th keycode or drops the
  reserved report byte.
- ESP32 BLE HID now uses the NimBLE input-report factory so the HID GATT service and report characteristic are registered.
- WROOM keyboard bench corrections in `src/VIA_ESP32_GPIO.h`,
  `src/VIA_ESP32_BLE.h` and
  `examples/ESP32_WROOM32_VIA_BLE/ESP32_WROOM32_VIA_BLE.ino`:
  return electrical GPIO level instead of double-inverting active-low
  inputs; send 8-byte HOGP reports; describe modifiers E0..E7; correct
  basic keycode defaults and guard the loop after initialization failure.
  Rows are now `{4,19,23,13,14}`, columns `{27,16,17,18,21,22}`;
  classic ESP32 strapping GPIO0/2/5/12/15 are unused. Existing NVS keymaps
  remain unchanged; BLE uses the board identity and bonding.
  Evidence: failing-before/passing-after polarity and framing regressions;
  gpio, hid, shared-server, keyboard and protocol native suites exit 0;
  shared path ASan/UBSan exit 0; WROOM compile 624135 bytes/39660 RAM;
  COM39 flash hashes verified and serial ready/HID1812+FF60 observed.
  Basic host keypress/remap is user-confirmed below. Impact: rewire old
  GPIO5/12/15 connections and re-pair after descriptor/identity changes.
  Rollback: USB-flash a known artifact with matching wiring; `58c8671`
  still has the old polarity/framing defects and is not a production fix.
  Runtime shared-server coverage replaces
  `tests/esp32_shared_server_contract_test.py` source-text assertions;
  native test link commands in `.github/workflows/ci.yml` include the
  matrix/keyboard/protocol path.

### WROOM Wi-Fi OTA
- Add authenticated ArduinoOTA to
  `examples/ESP32_WROOM32_VIA_BLE/WroomOTA.h` with bounded configuration
  parsing in `WroomOtaConfig.h` and service calls in the WROOM sketch.
  Wi-Fi connects/reconnects without blocking the keyboard scan loop.
  Serial provisioning saves one validated `airvia-ota/config` NVS blob;
  source and logs contain no provisioned credentials. OTA remains available
  even if keyboard initialization fails; held keys are released at upload start.
- Add `extras/wroom_ota.py`: hidden interactive Wi-Fi provisioning or
  non-echoed stdin automation, a separate generated OTA credential in
  Windows Credential Manager, serial status and IP-only firmware upload.
  The ESP32 core's uploader runs in-process, keeping authentication out of
  process arguments. `docs/PORTING.md` documents commands and recovery.
- Evidence: parser boundaries passed ASan/UBSan, three host validation
  tests passed, Windows Credential Manager store/read/delete smoke passed;
  combined build 1226171 bytes (93%), RAM64756 (19%); bootstrap COM39 flash
  completed with all hashes verified. Real network smoke (2026-10-25):
  IOTech1/IP192.168.1.103, wrong password rejected, authenticated upload
  exit0/OTA_UPLOAD_OK in54.90s, reboot app0->app1 with matching image MD5,
  post-OTA VIA13 and saved keymap readback passed. See
  `docs/testing/esp32-wroom-2026-10-20.md`; this same-image smoke does not
  certify firmware-schema migration or power-loss rollback.
- Impact: subsequent application `.bin` uploads can use the device IP;
  preserve OTA support and the dual-app partition layout. OTA does not
  erase keyboard NVS. Rollback/recovery remains USB flashing a compatible
  image without erase-all. Digest authentication is trusted-LAN tooling,
  not encrypted transport, signed firmware or production certification.

### WROOM hardware acceptance — 2026-10-25
- User confirmed that changing the VIA keymap and pressing physical keys
  works on the current ESP32-WROOM fixture. Record this as manual
  user-reported PASS, not an automated per-device key-down/key-up capture.
  `docs/testing/esp32-wroom-2026-10-20.md` distinguishes this result from
  the earlier automated UI remap/save/reconnect/restore and OTA evidence.
- Basic remap/physical-key smoke is accepted; exact tested positions,
  counts and release timing were not reported. Full-matrix, modifier,
  rollover, encoder, power-loss, soak and production gates remain open.
  This confirmation does not change firmware behavior or reset NVS;
  rollback/recovery instructions remain in `docs/PORTING.md`.

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
