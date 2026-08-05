# nice!nano v2 AirVIA BLE Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add compile-verified nice!nano v2 support that provides BLE HID typing, VIA v13 configuration through AirVIA FF60/FF61/FF62 GATT, and power-loss-resistant InternalFS persistence.

**Architecture:** Keep `via::Protocol`, `via::Matrix`, and `via::Keyboard` unchanged. Add isolated Adafruit Bluefruit adapters implementing existing transport/HID interfaces, a dual-slot InternalFS `Storage`, and a BLE-only 2x3 reference sketch with matching V3 JSON. The sketch owns Bluefruit lifecycle and advertising so HID and VIA services share one SoftDevice instance.

**Tech Stack:** C++11, Arduino, Adafruit nRF52 BSP 1.7.0, Bluefruit52Lib, SoftDevice S140, Adafruit LittleFS/InternalFS, nice!nano v2-compatible Pro Micro nRF52840 variant.

## Global Constraints

- Board: nice!nano v2 / Pro Micro-form-factor nRF52840.
- Arduino core: Adafruit nRF52 BSP `1.7.0`.
- Board variant: `somik123/Adafruit_nRF52_Arduino_ProMicro` commit `bd0fdcf124f59662d0184c39126e456f89dccd9c`.
- Compile FQBN: `adafruit:nrf52:promicronrf52840`.
- VIA packets are exactly `via::kPacketSize` (32 bytes).
- `Bluefruit.begin(1, 0)` is called exactly once by the sketch, never by an adapter.
- The sketch calls `Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH)` before `Bluefruit.begin()`.
- No changes to existing protocol semantics or behavior of RP2040, STM32F103, or ESP32-S3 adapters.
- No split transport, central mode, battery, deep sleep, RGB, encoder, USB dual mode, DFU command, or bundled board package.
- Use static/caller-owned memory; no per-packet heap allocation.
- Documentation says `experimental / compile-verified` until hardware acceptance is recorded.
- Existing native tests remain C++11-clean with `-Wall -Wextra -Werror`.

## File Map

- `src/VIA_nRF52_GPIO.h`: active-low Arduino matrix IO for nRF52.
- `src/VIA_nRF52_BLE.h`: `BLEHidAdafruit` keyboard adapter and LED callback bridge.
- `src/VIA_nRF52_BLE_ViaTransport.h/.cpp`: FF60 service and FF61/FF62 characteristics.
- `src/VIA_nRF52_InternalFS.h/.cpp`: dual-slot storage over caller-owned staging memory.
- `tests/fakes/Arduino.h`: native GPIO fake.
- `tests/fakes/bluefruit.h`: Bluefruit service, characteristic, HID, connection, mutex, and advertising fake.
- `tests/fakes/Adafruit_LittleFS.h`, `tests/fakes/InternalFileSystem.h`: in-memory file-system fake.
- `tests/nrf52_gpio_hid_test.cpp`: GPIO and HID contract.
- `tests/nrf52_ble_transport_test.cpp`: GATT and packet behavior.
- `tests/nrf52_internalfs_test.cpp`: dual-slot persistence and corruption recovery.
- `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino`: BLE-only 2x3 firmware.
- `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.json`: matching AirVIA definition.
- `.github/scripts/install-nice-nano-variant.sh`: pinned variant installation.
- `.github/workflows/ci.yml`: native adapter tests and Arduino compile gate.
- `README.md`, `docs/API.md`, `docs/PORTING.md`, `CHANGELOG.md`, `library.properties`: user and release documentation.

---

### Task 1: nRF52 GPIO and BLE HID Adapters

**Files:**
- Create: `src/VIA_nRF52_GPIO.h`
- Create: `src/VIA_nRF52_BLE.h`
- Create: `tests/fakes/Arduino.h`
- Create: `tests/fakes/bluefruit.h`
- Create: `tests/nrf52_gpio_hid_test.cpp`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: `via::MatrixIO`, `via::KeyboardHID`, `via::KeyboardReport`.
- Produces: `via::nrf52::MatrixIOArduino` and `via::nrf52::BLEKeyboardHID`.

- [ ] **Step 1: Write the failing GPIO/HID test**

Create `tests/nrf52_gpio_hid_test.cpp` that:

```cpp
#include <assert.h>
#include "VIA_nRF52_GPIO.h"
#include "VIA_nRF52_BLE.h"

int main() {
  via::nrf52::MatrixIOArduino io;
  io.inputPullup(D0);
  assert(FakeArduino::mode[D0] == INPUT_PULLUP);
  io.driveLow(D1);
  assert(FakeArduino::mode[D1] == OUTPUT && FakeArduino::value[D1] == LOW);
  io.release(D1);
  assert(FakeArduino::mode[D1] == INPUT_PULLUP);

  BLEHidAdafruit service;
  via::nrf52::BLEKeyboardHID hid(service);
  assert(hid.begin());
  Bluefruit.connectedResult = true;
  via::KeyboardReport report = {0x02, 0, {0x04, 0x05, 0, 0, 0, 0}};
  assert(hid.send(report));
  assert(service.lastModifier == 0x02);
  assert(service.lastKeys[0] == 0x04 && service.lastKeys[1] == 0x05);
  service.dispatchKeyboardLeds(0x03);
  uint8_t leds = 0;
  assert(hid.takeHostLeds(leds) && leds == 0x03);
  assert(!hid.takeHostLeds(leds));
}
```

The Bluefruit fake must expose deterministic connection/report/callback state and define only APIs used by production adapters.

- [ ] **Step 2: Run the test and confirm missing adapters**

Run:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_gpio_hid_test.cpp -o nrf52_gpio_hid_test
```

Expected: FAIL because `VIA_nRF52_GPIO.h` and `VIA_nRF52_BLE.h` do not exist.

- [ ] **Step 3: Implement the GPIO adapter**

```cpp
#pragma once
#if defined(ARDUINO_ARCH_NRF52) && defined(NRF52840_XXAA)
#include <Arduino.h>
#include "VIA_Matrix.h"
namespace via { namespace nrf52 {
class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) == LOW; }
  void delayMicroseconds(uint16_t us) override { ::delayMicroseconds(us); }
};
}}
#endif
```

- [ ] **Step 4: Implement the BLE HID adapter**

Create `BLEKeyboardHID` with these exact public methods:

```cpp
explicit BLEKeyboardHID(BLEHidAdafruit& service);
bool begin();
bool configured() const override;
bool send(const via::KeyboardReport& report) override;
bool sendComplete() override;
bool takeHostLeds(uint8_t& leds) override;
bool suspended() const override;
bool remoteWakeupAllowed() const override;
bool remoteWakeup() override;
```

`begin()` installs `BLEHidAdafruit::setKeyboardLedCallback()`, uses one static active pointer, and rejects any second active adapter. `send()` returns the result of `keyboardReport(report.modifiers, report.keys)`. Store LED callback state until consumed.

- [ ] **Step 5: Run focused tests**

Run:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_gpio_hid_test.cpp -o nrf52_gpio_hid_test && \
  ./nrf52_gpio_hid_test
```

Expected: PASS.

- [ ] **Step 6: Add the native CI command and commit**

Add a `nrf52-adapters` CI job containing the focused compile/run command.

```bash
git add src/VIA_nRF52_GPIO.h src/VIA_nRF52_BLE.h tests/fakes/Arduino.h \
  tests/fakes/bluefruit.h tests/nrf52_gpio_hid_test.cpp .github/workflows/ci.yml
git commit -m "feat(nrf52): add GPIO and BLE HID adapters"
```

### Task 2: Bluefruit AirVIA GATT Transport

**Files:**
- Create: `src/VIA_nRF52_BLE_ViaTransport.h`
- Create: `src/VIA_nRF52_BLE_ViaTransport.cpp`
- Create: `tests/nrf52_ble_transport_test.cpp`
- Modify: `tests/fakes/bluefruit.h`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: `via::Transport`, Bluefruit already initialized by the sketch.
- Produces: `via::nrf52::BLEViaTransport` with `begin`, transport methods, `connected`, `droppedPackets`, and `service`.

- [ ] **Step 1: Write failing GATT contract tests**

Test these exact behaviors:

```cpp
via::nrf52::BLEViaTransport transport;
assert(transport.begin("12345678901234567890123456789", 0x01020304));
assert(FakeBluefruit::serviceUuid == 0xFF60);
assert(FakeBluefruit::ff61Uuid == 0xFF61);
assert(FakeBluefruit::ff61FixedLength == via::kPacketSize);
assert(FakeBluefruit::ff62Uuid == 0xFF62);
assert(FakeBluefruit::ff62Value[0] == 0x01);
assert(FakeBluefruit::ff62Value[1] == 0x02);
assert(FakeBluefruit::ff62Value[2] == 0x03);
assert(FakeBluefruit::ff62Value[3] == 0x04);
```

Also dispatch 31-, 32-, and 33-byte writes. Assert only the 32-byte packet is received. Dispatch a second valid write before consumption and assert the first remains intact and `droppedPackets() == 1`. Assert send updates readable FF61 value; subscribed clients receive one notification; connected unsubscribed clients use read fallback; disconnected send returns false.

- [ ] **Step 2: Run and confirm missing transport**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_ble_transport_test.cpp \
  src/VIA_nRF52_BLE_ViaTransport.cpp -o nrf52_ble_transport_test
```

Expected: FAIL because transport files are absent.

- [ ] **Step 3: Define the public adapter**

```cpp
class BLEViaTransport : public via::Transport {
 public:
  BLEViaTransport();
  bool begin(const char* deviceName = "AirVIA", uint32_t firmwareVersion = 1);
  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override { return true; }
  bool connected() const;
  uint32_t droppedPackets() const;
  BLEService& service();
};
```

Members are `BLEService service_`, `BLECharacteristic ff61_`, `BLECharacteristic ff62_`, two 32-byte buffers, a static FreeRTOS mutex, pending flag, dropped counter, and static active pointer.

- [ ] **Step 4: Register the exact GATT contract**

In `begin()`:

```cpp
service_.begin();
ff61_.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE |
                    CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY);
ff61_.setPermission(SECMODE_OPEN, SECMODE_OPEN);
ff61_.setFixedLen(kPacketSize);
ff61_.setWriteCallback(onWrite);
ff61_.begin();
ff61_.write(responseBuffer_, kPacketSize);
ff62_.setProperties(CHR_PROPS_READ);
ff62_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
ff62_.setFixedLen(kPacketSize);
ff62_.begin();
ff62_.write(info, kPacketSize);
```

Do not call `Bluefruit.begin()`, set the global name, or start advertising.

- [ ] **Step 5: Implement synchronized packet flow**

The write callback takes the mutex without waiting. It rejects non-32-byte writes. If a request is pending, increment `droppedPackets_`; otherwise copy and mark pending. `receive()` takes the mutex, copies and clears pending. `send()` returns false when disconnected; otherwise writes response fallback and notifies only if notifications are enabled, returning the notify result or true for connected polling clients.

- [ ] **Step 6: Run focused tests and sanitizer**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_ble_transport_test.cpp \
  src/VIA_nRF52_BLE_ViaTransport.cpp -o nrf52_ble_transport_test && \
  ./nrf52_ble_transport_test
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_ble_transport_test.cpp \
  src/VIA_nRF52_BLE_ViaTransport.cpp -o nrf52_ble_transport_test_san && \
  ./nrf52_ble_transport_test_san
```

Expected: PASS.

- [ ] **Step 7: Update CI and commit**

```bash
git add src/VIA_nRF52_BLE_ViaTransport.h src/VIA_nRF52_BLE_ViaTransport.cpp \
  tests/fakes/bluefruit.h tests/nrf52_ble_transport_test.cpp .github/workflows/ci.yml
git commit -m "feat(nrf52): add AirVIA BLE transport"
```

### Task 3: Power-Loss-Resistant InternalFS Storage

**Files:**
- Create: `src/VIA_nRF52_InternalFS.h`
- Create: `src/VIA_nRF52_InternalFS.cpp`
- Create: `tests/fakes/Adafruit_LittleFS.h`
- Create: `tests/fakes/InternalFileSystem.h`
- Create: `tests/nrf52_internalfs_test.cpp`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: caller-owned staging buffer and Adafruit `InternalFS`.
- Produces: `via::nrf52::InternalFSStorage` implementing `via::Storage`.

- [ ] **Step 1: Write failing storage tests**

Instantiate with fixed memory:

```cpp
uint8_t staging[4096] = {};
via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
assert(storage.begin());
assert(storage.capacity() == sizeof(staging));
```

Test bounds, write/commit/restart/read, A/B generation alternation, corrupt-newest fallback, simulated file-write failure preserving the previous slot, erase, and generation wraparound. The fake filesystem must retain files across adapter instances and expose corruption/failure controls.

- [ ] **Step 2: Run and confirm missing storage**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test
```

Expected: FAIL because storage files are absent.

- [ ] **Step 3: Define record and public API**

```cpp
class InternalFSStorage : public via::Storage {
 public:
  InternalFSStorage(uint8_t* staging, size_t capacity);
  bool begin();
  size_t capacity() const override;
  bool read(size_t offset, uint8_t* output, size_t length) override;
  bool write(size_t offset, const uint8_t* input, size_t length) override;
  bool commit() override;
  bool erase() override;
};
```

Use a private 16-byte record header with magic `0x56494146` (`VIAF`), generation, payload length, and payload CRC32. Every committed wrapper record stores the full caller-provided staging capacity, avoiding ambiguity across the protocol's repeated offset-zero header writes.

- [ ] **Step 4: Implement begin and validation**

Mount `InternalFS` once. Read both files, require exact record length, matching capacity, magic, and CRC. Select newest with wrap-safe comparison:

```cpp
static bool newer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}
```

Load the selected payload into staging. If neither slot is valid, zero staging and start generation zero. Never call `InternalFS.format()`.

- [ ] **Step 5: Implement atomic slot commit**

Write header + full staging buffer to the inactive path, close, reopen, and validate it. Only then update in-memory active slot/generation. Keep the previous file. `erase()` removes both paths and zeros staging. All offset/length calculations use overflow-safe bounds checks:

```cpp
if (offset > capacity_ || length > capacity_ - offset) return false;
```

- [ ] **Step 6: Run normal and sanitizer tests**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test && ./nrf52_internalfs_test
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test_san && \
  ./nrf52_internalfs_test_san
```

Expected: PASS.

- [ ] **Step 7: Add CI commands and commit**

```bash
git add src/VIA_nRF52_InternalFS.h src/VIA_nRF52_InternalFS.cpp \
  tests/fakes/Adafruit_LittleFS.h tests/fakes/InternalFileSystem.h \
  tests/nrf52_internalfs_test.cpp .github/workflows/ci.yml
git commit -m "feat(nrf52): add dual-slot InternalFS storage"
```

### Task 4: nice!nano v2 Reference Firmware and JSON

**Files:**
- Create: `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino`
- Create: `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.json`
- Create: `tests/nice_nano_definition_test.py`

**Interfaces:**
- Consumes: all three adapter deliverables and existing core modules.
- Produces: a BLE-only 2x3 keyboard and exact AirVIA definition.

- [ ] **Step 1: Write the definition contract test**

Create `tests/nice_nano_definition_test.py` using only Python stdlib:

```python
import json
from pathlib import Path

path = Path("examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.json")
definition = json.loads(path.read_text(encoding="utf-8"))
assert definition["name"] == "AirVIA nice!nano 2x3"
assert definition["matrix"] == {"rows": 2, "cols": 3}
positions = [tuple(key["matrix"]) for key in definition["layouts"]["keymap"]]
assert positions == [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (1, 2)]
assert len(set(positions)) == 6
assert "encoders" not in definition
assert "lighting" not in definition
```

- [ ] **Step 2: Create the matching JSON**

```json
{
  "name": "AirVIA nice!nano 2x3",
  "vendorId": "0xFEED",
  "productId": "0x5284",
  "matrix": { "rows": 2, "cols": 3 },
  "layouts": {
    "keymap": [
      { "x": 0, "y": 0, "matrix": [0, 0] },
      { "x": 1, "y": 0, "matrix": [0, 1] },
      { "x": 2, "y": 0, "matrix": [0, 2] },
      { "x": 0, "y": 1, "matrix": [1, 0] },
      { "x": 1, "y": 1, "matrix": [1, 1] },
      { "x": 2, "y": 1, "matrix": [1, 2] }
    ]
  }
}
```

- [ ] **Step 3: Build the firmware state**

Use rows `D0,D1`, columns `D2,D3,D4`, 2 layers, six active-code entries, 4096-byte storage staging, and protocol load buffer sized as:

```cpp
static uint8_t loadBuffer[sizeof(keymap) + sizeof(uint32_t)] = {};
```

Use these exact values because the library classifies but does not define named QMK constants:

```cpp
static constexpr uint16_t KC_A = 0x0004;
static constexpr uint16_t KC_B = 0x0005;
static constexpr uint16_t KC_C = 0x0006;
static constexpr uint16_t KC_D = 0x0007;
static constexpr uint16_t KC_E = 0x0008;
static constexpr uint16_t KC_1 = 0x001E;
static constexpr uint16_t KC_2 = 0x001F;
static constexpr uint16_t KC_3 = 0x0020;
static constexpr uint16_t KC_4 = 0x0021;
static constexpr uint16_t KC_5 = 0x0022;
static constexpr uint16_t KC_TRNS = 0x0001;
static constexpr uint16_t MO_1 = 0x5221;
```

Layer 0 is A, B, C, D, E, MO(1). Layer 1 is 1, 2, 3, 4, 5, transparent.

- [ ] **Step 4: Initialize one Bluefruit stack and advertise both services**

The sketch setup sequence is:

```cpp
Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
Bluefruit.begin(1, 0);
Bluefruit.setName("AirVIA nice!nano");
Bluefruit.setTxPower(4);
Bluefruit.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
bleHid.begin();
bleKeyboard.begin();
bleVia.begin("AirVIA nice!nano", 0x00000001);
storage.begin();
protocol.begin(millis());
keyboard.begin();
startAdvertising();
```

Advertising includes general flags, transmit power, keyboard appearance, HID service, VIA FF60 service, name in scan response, restart-on-disconnect, fast interval then indefinite duration. `loop()` calls `protocol.task(now)` and `keyboard.task(now)` only.

- [ ] **Step 5: Run native definition and all existing tests**

```bash
python tests/nice_nano_definition_test.py
```

Then run all current CI native commands locally or invoke the repository's test script if available.

- [ ] **Step 6: Commit**

```bash
git add examples/nice_nano_v2_VIA_BLE tests/nice_nano_definition_test.py
git commit -m "example(nrf52): add nice nano AirVIA keyboard"
```

### Task 5: Pinned nice!nano Arduino Compile Gate

**Files:**
- Create: `.github/scripts/install-nice-nano-variant.sh`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: Adafruit nRF52 BSP 1.7.0 and pinned variant source.
- Produces: reproducible `adafruit:nrf52:promicronrf52840` Arduino compilation.

- [ ] **Step 1: Write the pinned installer**

The shell script uses `set -euo pipefail`, accepts Arduino data directory as its only argument, fetches exactly commit `bd0fdcf124f59662d0184c39126e456f89dccd9c`, copies `variants/pro_micro_nrf52840`, and inserts only the `promicronrf52840` board block into Adafruit BSP `boards.txt`. It must fail when expected paths or block markers are missing and must be idempotent.

- [ ] **Step 2: Test installer idempotence in a temporary fixture**

Create a temporary fake Adafruit core layout, run the installer twice, and assert one board block and one variant directory exist. Put this command in the CI job before real core installation or add a small shell-test step.

- [ ] **Step 3: Add Arduino CLI core installation**

Use:

```bash
arduino-cli config add board_manager.additional_urls \
  https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core update-index
arduino-cli core install adafruit:nrf52@1.7.0
bash .github/scripts/install-nice-nano-variant.sh "$HOME/.arduino15"
```

- [ ] **Step 4: Compile the exact board target**

```bash
arduino-cli compile --fqbn adafruit:nrf52:promicronrf52840 \
  --libraries . examples/nice_nano_v2_VIA_BLE
```

Expected: PASS. Do not substitute `feather52840`.

- [ ] **Step 5: Run the complete CI-equivalent suite and commit**

Run native tests, sanitizers, Arduino Uno compile, RP2040 compile, and nice!nano compile.

```bash
git add .github/scripts/install-nice-nano-variant.sh .github/workflows/ci.yml
git commit -m "ci(nrf52): compile nice nano BLE example"
```

### Task 6: Documentation, Metadata, and Release Evidence

**Files:**
- Modify: `README.md`
- Modify: `docs/API.md`
- Modify: `docs/PORTING.md`
- Modify: `CHANGELOG.md`
- Modify: `library.properties`
- Create: `docs/nice-nano-v2-hardware-checklist.md`

**Interfaces:**
- Produces: exact setup instructions and honest experimental support status.

- [ ] **Step 1: Update library metadata**

Bump `library.properties` to `version=0.5.0-experimental`. Add nRF52840/nice!nano v2 and Bluefruit AirVIA transport to `sentence`/`paragraph` without claiming hardware verification. Keep `architectures=*`; Bluefruit is supplied by the board core and does not need a Library Manager dependency entry. Update the README badge to `0.5.0-experimental`.

- [ ] **Step 2: Document installation and flashing**

README must specify Adafruit BSP 1.7.0, pinned variant commit, board selection `Pro Micro nRF52840`, UF2 reset/flash procedure, and experimental status. Include a 2x3 wiring table:

```text
Rows: D0, D1
Columns: D2, D3, D4
Each switch connects one row to one column; active-low with pull-ups.
```

- [ ] **Step 3: Document runtime workflow**

Explain OS BLE HID pairing, Chrome/Edge AirVIA connection, loading `nice_nano_v2_VIA_BLE.json`, sync/remap/save, reset/power-cycle verification, and recovery by removing the BLE bond and explicitly erasing VIA storage when required.

- [ ] **Step 4: Extend API and porting docs**

Document exact APIs for `MatrixIOArduino`, `BLEKeyboardHID`, `BLEViaTransport`, and `InternalFSStorage`. Add MTU, one-stack ownership, advertising payload, mutex, exact packet size, dual-slot record, and no-auto-format requirements.

- [ ] **Step 5: Update changelog and acceptance checklist**

Under Unreleased, list adapters, example, JSON, tests, and compile gate. Create a checkbox document covering flash, typing, FF60 discovery, sync, remap, persistence, reconnect, invalid packets, and corrupt-latest-slot recovery. Leave hardware boxes unchecked.

- [ ] **Step 6: Run final verification**

Run:

```bash
git diff --check
arduino-lint --library-manager update .
```

Run the full native, sanitizer, and Arduino compile suite from Tasks 1-5. Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add README.md docs/API.md docs/PORTING.md CHANGELOG.md library.properties \
  docs/nice-nano-v2-hardware-checklist.md
git commit -m "docs(nrf52): document nice nano BLE support"
```

## Final Acceptance

- Existing platform tests and examples remain green.
- Native nRF52 adapter and storage tests pass with warnings-as-errors.
- Sanitizer runs pass.
- Exact nice!nano-compatible FQBN compiles in CI.
- Firmware and JSON matrix/layer contract matches.
- No adapter initializes Bluefruit globally.
- Documentation remains experimental until physical checklist completion.
