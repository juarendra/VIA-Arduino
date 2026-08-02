# AirVIA BLE Transport Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create one NimBLE GATT transport adapter (`VIA_ESP32S3_BLE_ViaTransport`) implementing `via::Transport` so AirVIA web app can configure VIA keyboards over BLE.

**Architecture:** Single `.h`/`.cpp` file pair with static buffers and NimBLE callbacks. Zero changes to existing portable core or adapter files.

**Tech Stack:** C++11, ESP32-S3 Arduino, NimBLE-Arduino, `via::Transport` interface.

## Global Constraints

- MIT-licensed project code.
- C++11 compatibility; no heap allocation.
- Zero changes to existing source files.
- One singleton instance per device; static buffers follow the same pattern as `VIA_TinyUSB_RawHID`.
- `via::Protocol`, `via::Keyboard`, `via::Matrix` untouched.

---

### Task 1: BLE Via Transport Adapter

**Files:**
- Create: `src/VIA_ESP32S3_BLE_ViaTransport.h`
- Create: `src/VIA_ESP32S3_BLE_ViaTransport.cpp`

**Interfaces:**
- Consumes: `<NimBLEDevice.h>`, `<NimBLEServer.h>`
- Consumes: `via::Transport` (`VIA_Protocol.h`)
- Produces: `class via::esp32s3::BLEViaTransport : public via::Transport`

- [ ] **Step 1: Write header**

```cpp
#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

#include "VIA_Protocol.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>

namespace via {
namespace esp32s3 {

class BLEViaTransport : public via::Transport {
 public:
  bool begin(const char* deviceName = "AirVIA",
             uint32_t fwVersion = 0x00000001);
  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override;
  bool connected() const;

 private:
  static void onFF61Write(NimBLECharacteristic* c);
  static void onFF61Notify(NimBLECharacteristic* c);

  NimBLEServer* server_ = nullptr;
  NimBLEService* service_ = nullptr;
  NimBLECharacteristic* ff61_ = nullptr;
  NimBLECharacteristic* ff62_ = nullptr;

  static uint8_t rx_buffer_[kPacketSize];
  static volatile bool rx_ready_;
  static volatile bool tx_complete_;
  static BLEViaTransport* active_;
  static uint8_t response_buffer_[kPacketSize];
};

}  // namespace esp32s3
}  // namespace via

#endif
```

- [ ] **Step 2: Implement**

`begin()`: calls `NimBLEDevice::init(deviceName)`, creates server,
creates FF60 service, creates FF61 characteristic (Read|Write|Notify,
32B fixed, write callback `onFF61Write`, notify callback
`onFF61Notify`), creates FF62 characteristic (Read only, 32B with
firmware version big-endian + device name), starts service, starts
advertising.

`receive()`: if active and `rx_ready_`, noInterrupts copy from
`rx_buffer_` to packet, clear `rx_ready_`, return true.

`send()`: if active and connected, copy packet to `response_buffer_`,
call `ff61_->notify(response_buffer_, kPacketSize)`, return true.

`sendComplete()`: return `tx_complete_`; clear it after read.

Callbacks: `onFF61Write` copies 32B from characteristic value to
`rx_buffer_`, sets `rx_ready_`. `onFF61Notify` sets `tx_complete_`.

- [ ] **Step 3: Commit and push**

```bash
git add src/VIA_ESP32S3_BLE_ViaTransport.h src/VIA_ESP32S3_BLE_ViaTransport.cpp
git commit -m "feat(airvia): add BLE GATT VIA transport adapter"
git push
```

### Task 2: Update Example Sketch

**Files:**
- Modify: `examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino`

- [ ] **Step 1: Add BLEViaTransport to sketch**

Include `VIA_ESP32S3_BLE_ViaTransport.h`. Instantiate `BLEViaTransport`
and call `begin()`. In `loop()`, poll `bleVia.receive()` and route
through `protocol.process()`. Keep existing BLE typing and USB paths.

- [ ] **Step 2: Commit and push**

```bash
git add examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino
git commit -m "example(airvia): integrate BLE VIA transport"
git push
```

### Task 3: Docs

**Files:**
- Modify: `docs/API.md`
- Modify: `README.md`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Document new module**

Add `VIA_ESP32S3_BLE_ViaTransport` to API.md adapters section. Add
AirVIA BLE transport mention to README. Add changelog entry.

- [ ] **Step 2: Commit and push**

```bash
git add docs/API.md README.md CHANGELOG.md
git commit -m "docs(airvia): document BLE GATT VIA transport"
git push
```
