# nice!nano v2 AirVIA BLE Design

Date: 2026-08-05
Status: Approved

## Goal

Add an experimental Arduino adapter set for nice!nano v2 that exposes one nRF52840 as both a BLE HID keyboard and an AirVIA-compatible VIA v13 configuration device. Ship a small hardware-test sketch and matching V3 JSON definition, while preserving every existing platform adapter and the transport-independent protocol core.

## Supported Environment

- Board: nice!nano v2 / Pro Micro-form-factor nRF52840.
- Arduino core: Adafruit nRF52 BSP `1.7.0`.
- Board variant: `somik123/Adafruit_nRF52_Arduino_ProMicro` pinned at commit `bd0fdcf124f59662d0184c39126e456f89dccd9c`.
- Arduino FQBN after variant installation: `adafruit:nrf52:promicronrf52840`.
- BLE stack: Adafruit Bluefruit / SoftDevice S140.
- Status: compile-verified and protocol-fixture-verified until real nice!nano v2 hardware acceptance passes.

The variant is installed externally and pinned in CI; it is not copied into this library. This avoids pretending nice!nano v2 is an officially supported Adafruit board and avoids maintaining a forked BSP.

## Scope

Add:

- Bluefruit VIA GATT transport.
- Bluefruit BLE keyboard HID adapter.
- Arduino GPIO matrix adapter for nRF52.
- InternalFS dual-slot storage adapter.
- A BLE-only 2x3 keyboard example for nice!nano v2.
- A matching VIA V3 JSON definition.
- Native adapter tests and an Arduino CLI compile gate.
- Installation, flashing, wiring, pairing, and AirVIA documentation.

Do not add split-keyboard transport, central mode, battery reporting, deep sleep, RGB, encoder input, USB/VIA dual mode, DFU commands, or a board package. These can follow after the basic BLE HID + VIA path is hardware-verified.

## Architecture

```text
2x3 matrix -> via::Matrix -> via::Keyboard -> nrf52::BLEKeyboardHID -> BLE HID host
                                  |
AirVIA browser <-> FF60/FF61/FF62 <- nrf52::BLEViaTransport <- via::Protocol
                                  |
                         nrf52::InternalFSStorage
```

The existing `via::Protocol`, `via::Matrix`, and `via::Keyboard` remain unchanged. Platform code implements their existing interfaces.

## Bluefruit Ownership

The sketch owns global Bluefruit lifecycle:

1. Configure peripheral bandwidth before initialization.
2. Call `Bluefruit.begin(1, 0)` exactly once.
3. Set name, transmit power, appearance, and Just Works security policy.
4. Begin `BLEHidAdafruit`.
5. Begin `BLEViaTransport`, which registers FF60/FF61/FF62 but does not initialize Bluefruit or start advertising.
6. Advertise HID and FF60 services; put the device name in scan response.

This prevents two adapters from independently initializing or advertising the same SoftDevice stack.

Recommended initialization:

```cpp
Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
Bluefruit.begin(1, 0);
Bluefruit.setName("AirVIA nice!nano");
Bluefruit.setTxPower(4);
Bluefruit.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
bleHid.begin();
bleVia.begin("AirVIA nice!nano", 0x00000001);
startAdvertising();
```

`BANDWIDTH_HIGH` is required so the negotiated ATT MTU can carry the 32-byte VIA characteristic value without relying on the default 23-byte MTU.

## VIA GATT Transport

Create `via::nrf52::BLEViaTransport` in `VIA_nRF52_BLE_ViaTransport.h/.cpp`.

### Service Contract

- Service `0xFF60`.
- Characteristic `0xFF61`: read, write, write-without-response, notify; fixed length 32.
- Characteristic `0xFF62`: read-only; fixed length 32.
- Open characteristic permissions, matching the existing ESP32 AirVIA transport.

FF62 layout:

- Bytes 0-3: firmware version, big-endian.
- Bytes 4-31: UTF-8 device name, truncated to 28 bytes and zero padded.

FF61 behavior:

- Accept only writes whose final length is exactly `via::kPacketSize`.
- Copy one pending request into an owned buffer.
- Reject and count a new packet while one remains pending; never overwrite it.
- `receive()` atomically consumes the pending packet.
- `send()` stores the last response, updates the readable value, and notifies only when connected and subscribed.
- `sendComplete()` returns true after Bluefruit accepts the synchronous notify call.
- FF61 read returns the last 32-byte response as a polling fallback.

### Concurrency

Bluefruit callbacks and Arduino `loop()` may run in different FreeRTOS tasks. Protect request state with a statically allocated FreeRTOS mutex. Do not rely on `volatile`, interrupt disabling, or heap allocation per packet.

The adapter is single-instance because Bluefruit callbacks are C function pointers. `begin()` fails if another active instance exists.

Expose:

```cpp
bool begin(const char* deviceName = "AirVIA", uint32_t firmwareVersion = 1);
bool receive(uint8_t packet[kPacketSize]) override;
bool send(const uint8_t packet[kPacketSize]) override;
bool sendComplete() override;
bool connected() const;
uint32_t droppedPackets() const;
BLEService& service();
```

## BLE Keyboard HID

Create header-only `via::nrf52::BLEKeyboardHID` wrapping a caller-owned `BLEHidAdafruit`.

- `configured()`: true when Bluefruit has a peripheral connection.
- `send()`: call `keyboardReport(modifiers, keys)` with the existing six-key report.
- `sendComplete()`: true because the Bluefruit call synchronously accepts or rejects the report.
- `takeHostLeds()`: consume the most recent keyboard LED callback value once.
- `suspended()`: true while disconnected.
- Remote wake methods: false.

Use one active adapter pointer for the static host-LED callback. `begin()` installs the callback and rejects a second active adapter.

## GPIO Matrix Adapter

Create header-only `via::nrf52::MatrixIOArduino` behind `ARDUINO_ARCH_NRF52`.

- Use Arduino symbolic pins (`D0`, `D1`, etc.) supplied by the nice!nano-compatible variant.
- Use `INPUT_PULLUP`, active-low scan, output-low drive, and release back to input-pullup.
- Do not embed Nordic P0/P1 hardware pin numbers in library code.

## InternalFS Storage

Create `via::nrf52::InternalFSStorage`, implementing `via::Storage` with a caller-provided staging buffer and capacity. The example uses 4096 bytes.

Use two files:

- `/via_a.bin`
- `/via_b.bin`

Each record contains:

```text
magic (4) | generation (4) | payload length (4) | payload CRC32 (4) | payload
```

Behavior:

- `begin()` mounts InternalFS, validates both records, and loads the newest valid generation into staging.
- If the latest slot is corrupt, use the older valid slot.
- `read()` and `write()` operate only inside the staging buffer with strict bounds.
- `commit()` writes the inactive slot completely, closes it, reopens and verifies header/CRC, then marks it active in memory. The previous file remains valid.
- `erase()` removes both files and zeroes staging.
- Generation comparison handles unsigned wraparound.
- Never format InternalFS automatically after a mount or record failure; formatting could erase unrelated application data.

This adapter provides power-loss recovery at the record level without modifying the protocol storage schema.

## nice!nano v2 Example

Create `examples/nice_nano_v2_VIA_BLE/` with:

- `nice_nano_v2_VIA_BLE.ino`
- `nice_nano_v2_VIA_BLE.json`

The sketch is BLE-only and intentionally small:

- Matrix: 2 rows x 3 columns.
- Layers: 2.
- Pins: rows `D0`, `D1`; columns `D2`, `D3`, `D4`.
- Six physical switches.
- Layer 0: A, B, C, D, E, momentary Layer 1.
- Layer 1: 1, 2, 3, 4, 5, transparent.
- No macros, encoders, lighting, battery, or sleep.
- 4096-byte storage staging buffer plus correctly sized protocol load buffer.
- `Protocol` uses `BLEViaTransport` directly and runs through `protocol.task(millis())`.

The JSON must exactly match the 2x3 matrix, contain six unique matrix coordinates, and omit unsupported feature declarations.

## Advertising

The example advertising setup must:

- Add general discoverable flags.
- Add transmit power and keyboard appearance.
- Advertise both HID and FF60 services.
- Put the full name in scan response.
- Restart advertising after disconnect.
- Use indefinite advertising after a normal fast interval/timeout phase.

## Testing

### Native Tests

Add Bluefruit fakes and tests covering:

- FF60/FF61/FF62 UUIDs and properties.
- Fixed 32-byte lengths and open permissions.
- Exact request acceptance.
- Short and oversized request rejection.
- Pending request cannot be overwritten.
- Request consumption.
- Last-response read fallback.
- Notify only when connected and subscribed.
- FF62 big-endian version, name truncation, and zero padding.
- BLE HID report forwarding and host LED consumption.

Add InternalFS fakes and tests covering:

- Empty startup.
- Bounds rejection.
- Commit then read after restart.
- Newest valid generation selection.
- Corrupt newest slot fallback.
- Failed write preserves old slot.
- Erase removes both records.
- Generation wraparound.

All native tests compile as C++11 with `-Wall -Wextra -Werror`; storage tests also run under ASan/UBSan.

### Arduino Compile Gate

CI must:

1. Install Adafruit nRF52 BSP `1.7.0`.
2. Fetch nice!nano-compatible variant commit `bd0fdcf124f59662d0184c39126e456f89dccd9c`.
3. Install its `promicronrf52840` board definition and variant into the CI core.
4. Compile `examples/nice_nano_v2_VIA_BLE` using `adafruit:nrf52:promicronrf52840`.

Pin every external version and commit. Do not silently compile against Feather.

## Documentation

Update README, API docs, porting docs, library metadata, and changelog with:

- Experimental nice!nano v2 support.
- Exact Adafruit BSP and pinned variant installation.
- Board selection and symbolic pin mapping.
- UF2 bootloader flashing steps.
- 2x3 wiring table.
- BLE pairing and AirVIA connection procedure.
- Loading the included JSON definition.
- Recovery: reset, remove BLE bond, reload definition, erase VIA storage explicitly.
- No claim of hardware verification until acceptance is recorded.

## Hardware Acceptance

Before calling support hardware-verified, record:

1. Compile and UF2 flash on nice!nano v2.
2. BLE HID pairing and six-key typing.
3. AirVIA discovery of FF60.
4. Protocol version and full keymap synchronization.
5. Key remap acknowledgment.
6. Persistence after reset and power cycle.
7. Disconnect/reconnect recovery.
8. Invalid packet rejection without crash or state corruption.
9. Older storage slot recovery after intentionally corrupting the latest slot.

Until then, documentation uses `experimental / compile-verified`.

## Definition of Done

- Existing protocol and platform adapters remain unchanged in behavior.
- nice!nano adapter tests pass.
- nice!nano Arduino example compiles against the pinned real board variant.
- Example JSON parses and exactly matches firmware matrix/layers.
- Existing CI remains green.
- Documentation states limitations accurately.
