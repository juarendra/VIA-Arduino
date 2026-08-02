# AirVIA BLE Transport Adapter Design

## Status

Approved design. Phase 0.4-airvia. One new file, zero existing files changed.

## Goal

Add a NimBLE GATT transport adapter that tunnels VIA protocol v13 packets
over BLE, enabling the AirVIA web app to configure keyboard keymaps
wirelessly without USB. The adapter coexists with the existing BLE HID
keyboard typing adapter.

## Reference Platform

- SoC: ESP32-S3-WROOM-1.
- BLE stack: NimBLE-Arduino v2.x.
- Arduino core: `espressif/esp32` v3.0.x.
- Coexists with: `VIA_ESP32S3_BLE.h` (BLE keyboard typing), TinyUSB (USB
  VIA + USB keyboard).

## Architecture

```text
ESP32-S3
  ├── NimBLE GATT Server
  │     ├── Service UUID: 0000FF60-0000-1000-8000-00805F9B34FB
  │     │     ├── Char FF61 (0000FF61-...): Read|Write|Notify, 32B fixed
  │     │     │     Write → VIA command packet (32 bytes)
  │     │     │     Notify → VIA response packet (32 bytes)
  │     │     │     Read → last response (for polling fallback)
  │     │     └── Char FF62 (0000FF62-...): Read
  │     │           firmwareVersion (4B uint32 BE) + deviceName (UTF-8 ≤28B)
  │     └── BLE advertising device name: "AirVIA" + user suffix
  │
  ├── BLEViaTransport : via::Transport   [NEW — one .h/.cpp file]
  │     ├── receive()  → copy from rx_buffer_, clear rx_ready_
  │     ├── send()     → notify FF61 with 32B response
  │     └── sendComplete() → true after notify callback fires
  │
  ├── via::Protocol (unchanged)
  │     └── protocol.process(packet, millis()) via BLEViaTransport
  │
  └── via::Keyboard + BLE HID typing (unchanged, coexists)
```

## GATT Details

| Item | Value |
|---|---|
| Service UUID | `0000FF60-0000-1000-8000-00805F9B34FB` |
| FF61 UUID | `0000FF61-0000-1000-8000-00805F9B34FB` |
| FF61 properties | Read \| Write \| Notify |
| FF61 size | 32 bytes fixed |
| FF62 UUID | `0000FF62-0000-1000-8000-00805F9B34FB` |
| FF62 properties | Read |
| FF62 value | `uint32_t fwVersion (big-endian)` + `char deviceName[≤28]` (UTF-8) |

## VIA Protocol Over BLE

The VIA packet format is identical to USB Raw HID:

- Packet: 32 bytes fixed.
- Command ID in byte 0.
- Response: same 32-byte buffer, modified in-place.
- Error: byte 0 set to `0xFF`.
- Flow: app writes 32B to FF61 → firmware calls `protocol.process()` →
  firmware notifies 32B response to FF61.

Minimal supported commands are defined by `VIA_Protocol.cpp`. No changes
to the protocol core.

## Interface

```cpp
namespace via {
namespace esp32s3 {

class BLEViaTransport : public via::Transport {
 public:
  bool begin(const char* deviceName = "AirVIA",
             uint32_t fwVersion = 0x00000001);

  bool receive(uint8_t packet[via::kPacketSize]) override;
  bool send(const uint8_t packet[via::kPacketSize]) override;
  bool sendComplete() override;

  bool connected() const;

 private:
  static void onFF61Write(NimBLECharacteristic* c);
  static void onFF61Notify(NimBLECharacteristic* c);

  NimBLEServer* server_ = nullptr;
  NimBLECharacteristic* ff61_ = nullptr;
  NimBLECharacteristic* ff62_ = nullptr;

  static uint8_t rx_buffer_[32];
  static volatile bool rx_ready_;
  static volatile bool tx_complete_;
  static BLEViaTransport* active_;
};

}  // namespace esp32s3
}  // namespace via
```

Static buffers and callbacks follow the same singleton pattern used by
`VIA_TinyUSB_RawHID`. Exactly one instance per device.

## Packet Flow

```text
App (AirVIA)                          Firmware (ESP32-S3)
     |                                      |
     |-- BLE GATT Write FF61 (32B) -------->|
     |                                      | NimBLE callback: copy to rx_buffer_
     |                                      | set rx_ready_ = true
     |                                      |
     |                              loop(): receive() → copy from rx_buffer_
     |                                      protocol.process(packet, now)
     |                                      send(response)
     |                                      |
     |<-- BLE GATT Notify FF61 (32B) ------|
     |                                      | NimBLE notify callback: tx_complete_ = true
```

## Coexistence With BLE HID Keyboard

The sketch runs two separate BLE roles on the same ESP32-S3:

- **GATT server** (this adapter): VIA configuration tunnel. App connects
  as GATT client.
- **BLE HID device** (`VIA_ESP32S3_BLE.h`): keyboard typing reports. Host
  OS connects as HID host.

Both use the same NimBLE stack. NimBLE supports multiple GATT services
and concurrent connections. No conflict.

## Sketch Integration

```cpp
// Existing
BleKeyboard bleKeyboard("VIA Keyboard", "VIA-Arduino", 100);
via::esp32s3::BleKeyboardHID bleHid(bleKeyboard);

// New
via::esp32s3::BLEViaTransport bleVia;
bleVia.begin("AirVIA KB", 0x00000001);

void loop() {
  uint32_t now = millis();

  // VIA over BLE
  uint8_t packet[via::kPacketSize];
  if (bleVia.receive(packet)) {
    protocol.process(packet, now);
    bleVia.send(packet);
  }

  // VIA over USB (if plugged in)
  protocol.task(now);

  // Keyboard typing (BLE HID)
  keyboard.task(now);
}
```

## Non-Requirements

- No BLE security pairing — Just Works mode.
- No automatic disconnect/reconnect handling — app side responsibility.
- No GATT Device Information service (model number, serial, etc.).
- No firmware update over BLE.
- No BLE bond storage or multi-client support.
- No changes to `VIA_Protocol`, `VIA_Keyboard`, `VIA_Matrix`, or any
  existing source file.

## Testing

- Compile: ESP32-S3 + NimBLE + this adapter → links.
- Host test: verify `receive()`/`send()`/`sendComplete()` contracts with
  a fake NimBLE backend.
- Hardware: flash to ESP32-S3, advertise, AirVIA app discovers service,
  reads FF62, writes FF61, receives notify response.
