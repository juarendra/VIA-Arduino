# nice!nano v2 CI and Hardware Acceptance Design

Date: 2026-08-05
Status: Approved

## Goal

Finish the experimental nice!nano v2 AirVIA BLE work by making the pinned
Arduino build reproducibly green, aligning the reference firmware with the
approved architecture, and completing physical acceptance on one nice!nano v2
wired as a 2x3 switch matrix.

## Starting Point

- The base design is
  `docs/superpowers/specs/2026-08-05-nice-nano-v2-airvia-ble-design.md`.
- Adafruit nRF52 BSP is pinned at `1.7.0`.
- The board variant is pinned at commit
  `bd0fdcf124f59662d0184c39126e456f89dccd9c`.
- The compile target is `adafruit:nrf52:promicronrf52840`.
- Commit `271d28c` is the latest pushed commit.
- The latest CI run reaches the nice!nano link step and fails because
  `VIA_nRF52_InternalFS.cpp` references unavailable symbol `crc32_compute`.
- A local CRC32 replacement and corresponding test adjustment are currently
  staged but uncommitted. Implementation starts by reviewing this exact diff,
  not by recreating it.
- Two existing untracked native binaries, `nrf52_ble_transport_test` and
  `nrf52_ble_transport_test_san`, are unrelated and must not be committed.

## Scope

Included:

- Remove the unavailable CRC linker dependency.
- Add deterministic CRC regression coverage.
- Restore all native, sanitizer, AVR, RP2040, and pinned nice!nano CI gates.
- Audit and correct the nice!nano sketch against the approved BLE-only design.
- Close adapter contract gaps that can invalidate hardware acceptance.
- Produce a traceable firmware artifact from the pinned toolchain.
- Flash and test a nice!nano v2 with six physical switches.
- Record evidence for BLE HID, AirVIA, persistence, reconnect, invalid-packet,
  and corrupt-newest-slot recovery.
- Update support status only after every physical check passes.

Excluded:

- Split keyboard transport, BLE central mode, battery reporting, deep sleep,
  RGB, encoders, DFU commands, and board-package maintenance.
- Refactoring unrelated protocol or platform code.
- Updating GitHub Actions solely to silence Node runtime deprecation warnings.

## Architecture

The finished firmware keeps one direct path per responsibility:

```text
2x3 matrix -> via::Matrix -> via::Keyboard -> BLEKeyboardHID -> BLE HID host

AirVIA <-> FF60/FF61/FF62 <-> BLEViaTransport <-> via::Protocol
                                              |
                                      InternalFSStorage
```

The sketch owns Bluefruit initialization and advertising. `via::Protocol`
owns request polling through `BLEViaTransport`; the sketch does not manually
receive, process, and resend VIA packets. TinyUSB Raw HID and USB keyboard
objects are removed from this BLE-only reference.

## Compile Stabilization

### CRC Contract

`InternalFSStorage` must be self-contained and must not assume that the BSP
links Nordic SDK helper `crc32_compute`.

Use reflected CRC32 with:

- Polynomial: `0xEDB88320`.
- Initial state: `0xFFFFFFFF`.
- Final XOR: none.
- Incremental chunking: the previous state becomes the next chunk's initial
  state.

Known states:

- `"123456789"`: `0x340BC6D9`.
- 4096 zero bytes: `0x38E3FFEE`.

Tests must validate production-generated record CRC, not only duplicate the
algorithm in test code. The native storage test reads a committed record
header from the fake filesystem and compares its CRC with a fixed known value.

### Portable Library Build

All nRF52-only headers and source bodies remain behind
`ARDUINO_ARCH_NRF52 && NRF52840_XXAA`. Arduino's library builder compiles every
source file, so AVR and RP2040 builds must see empty nRF52 translation units
without requiring Bluefruit or InternalFS headers.

The Bluefruit fakes must match the real BSP API signatures used by production
code. No fake-only typedef, method name, or callback shape may require a cast
or branch in production.

## Firmware Conformance

### Initialization Order

The reference sketch initializes exactly once in this order:

1. Configure `BANDWIDTH_HIGH`.
2. Call `Bluefruit.begin(1, 0)`.
3. Set device name, TX power, appearance, and no-I/O Just Works policy.
4. Begin `BLEHidAdafruit`.
5. Begin `BLEKeyboardHID` to install the host-LED callback.
6. Begin `BLEViaTransport` to register FF60/FF61/FF62.
7. Mount `InternalFSStorage`.
8. Begin `via::Protocol` and `via::Keyboard`.
9. Advertise HID and FF60 services indefinitely after the fast phase.

No adapter calls `Bluefruit.begin()` or starts advertising.

### Firmware State

- Matrix: 2 rows x 3 columns.
- Rows: `D0`, `D1`.
- Columns: `D2`, `D3`, `D4`.
- Layers: 2.
- Layer 0: A, B, C, D, E, MO(1).
- Layer 1: 1, 2, 3, 4, 5, transparent.
- Storage staging: 4096 bytes.
- Protocol load buffer: `sizeof(keymap) + sizeof(uint32_t)`.
- Transport: `BLEViaTransport` passed directly to `via::Protocol`.
- No USB, macro, encoder, lighting, battery, or sleep feature.

### FF60 Service

- FF61 remains fixed at 32 bytes with read, write, write-without-response, and
  notify properties.
- FF62 bytes 0-3 contain the big-endian firmware version.
- FF62 bytes 4-31 contain the UTF-8 device name, truncated to 28 bytes and
  zero-padded.
- One pending request is preserved; a second request increments the dropped
  count and never overwrites the first.
- Request state is protected by a statically allocated FreeRTOS mutex. Failure
  to acquire the mutex drops the new request without reading or writing shared
  packet state.
- `begin()` rejects a second active transport instance.

### InternalFS Durability

The continuation keeps existing paths `/via_a.dat` and `/via_b.dat`; changing
paths provides no acceptance value and could strand data from experimental
testers. This path choice supersedes the `.bin` spelling in the original
design, while the record format remains unchanged.

`commit()` must close and reopen the inactive slot, validate its header, read
the full payload back in chunks, and compare payload CRC before advancing the
active generation. A failed mount, open, write, read, or CRC verification
returns false and leaves the previous valid slot selectable. Storage never
formats InternalFS automatically.

## CI Gates

One pushed commit is accepted only when GitHub Actions confirms:

1. Arduino lint passes.
2. Existing native tests pass with `-Wall -Wextra -Werror`.
3. nRF52 GPIO/HID, GATT, and InternalFS tests pass.
4. BLE transport and InternalFS sanitizer runs pass.
5. Arduino Uno `Protocol_Self_Test` compiles.
6. RP2040 Raw HID reference compiles.
7. Variant installer idempotence passes.
8. The pinned nice!nano example compiles for
   `adafruit:nrf52:promicronrf52840`.

Because local `g++` and `arduino-cli` are unavailable, GitHub Actions is the
authoritative compile environment. Each failed run is inspected before any
new change; unrelated fixes are not bundled.

## Firmware Artifact

The accepted CI build uploads the nice!nano firmware output as an artifact.
Evidence records:

- Git commit SHA.
- GitHub Actions run URL.
- Adafruit BSP version `1.7.0`.
- Variant commit.
- FQBN.
- Firmware filename and SHA-256.

The artifact is the only image used for physical acceptance. Rebuilding with
different versions invalidates the evidence.

## Hardware Fixture

Available fixture:

- One nice!nano v2.
- USB cable and bootloader access.
- Six switches wired as a 2x3 matrix.
- Rows on `D0`, `D1`.
- Columns on `D2`, `D3`, `D4`.
- Chrome or Edge with Web Bluetooth and the current AirVIA deployment.

Before power-on, verify continuity and confirm no row/column short to VCC or
ground. Physical tests use USB power first; a battery is not required.

## Hardware Acceptance Flow

### Flash and Boot

1. Download the accepted CI artifact.
2. Verify SHA-256 against recorded evidence.
3. Enter nice!nano UF2 bootloader and flash the artifact.
4. Confirm normal reboot and BLE advertising as `AirVIA nice!nano`.

### BLE HID

1. Pair through the operating system.
2. Press all six switches on layer 0 and observe A, B, C, D, E, and no literal
   output from MO(1).
3. Hold MO(1), press the first five switches, and observe 1, 2, 3, 4, 5.
4. Confirm no stuck key after release or reconnect.

### AirVIA

1. Load `nice_nano_v2_VIA_BLE.json`.
2. Connect through Web Bluetooth and confirm FF60 discovery.
3. Read VIA protocol version `0x000D`.
4. Synchronize all 12 layer entries.
5. Remap one key, observe acknowledgment, and verify new typing behavior.

### Persistence and Reconnect

1. Disconnect AirVIA and reconnect without power cycling.
2. Reset the board and verify the remap remains.
3. Remove power for at least ten seconds, reconnect, and verify the remap
   remains.
4. Remove and recreate the OS BLE bond, then confirm HID and AirVIA recover.

### Invalid Packet

Serve `tests/hardware/nice_nano_ble_packet_test.html` from localhost and use its
Web Bluetooth button to send 31-byte, 33-byte, then valid 32-byte writes to
FF61. The utility reports characteristic write results and reads FF61 after the
valid request; it does not contain application or firmware logic. Confirm no
invalid write is treated as a VIA packet, valid traffic still works, and the
saved keymap remains intact after reset.

### Corrupt-Newest Recovery

Use `tests/hardware/nice_nano_storage_corruptor/` as a dedicated acceptance
sketch. It mounts InternalFS, reads both 16-byte slot headers, selects the
wrap-safe newest generation, and overwrites that slot with an invalid header.
It must not format the filesystem or modify the older slot. Reflash the
accepted firmware without erasing InternalFS and confirm the older keymap is
loaded and AirVIA remains usable.

The corruptor is a test fixture under `tests/hardware/`, not a production
command or permanent firmware backdoor.

## Evidence and Status

Record every check in `docs/nice-nano-v2-hardware-checklist.md` with date,
board identity, host OS/browser, commit SHA, Actions URL, artifact SHA-256, and
observed result. Failed checks remain unchecked and include the exact symptom.

Only after all checks pass:

- Change documentation from `experimental / compile-verified` to
  `experimental / hardware-verified on nice!nano v2`.
- Keep the feature experimental; one successful fixture does not establish
  broad production support.

## Definition of Done

- CI is green at the recorded commit.
- The pinned nice!nano artifact is available and hashed.
- The firmware matches the BLE-only architecture and 2x3 JSON definition.
- Native tests cover CRC, real Bluefruit callback shape, FF62 payload,
  concurrency rejection, and full post-write storage verification.
- All physical acceptance checks pass on the available board and six-switch
  matrix.
- Evidence is recorded without claiming broader support.
- No unrelated feature, dependency, or generated native binary is committed.
