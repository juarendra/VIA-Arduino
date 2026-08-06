# nice!nano v2 CI and Hardware Acceptance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a reproducibly green, pinned nice!nano v2 firmware build and complete documented physical acceptance for BLE HID, AirVIA, persistence, reconnect, invalid packets, and corrupt-slot recovery.

**Architecture:** Keep the portable protocol, matrix, and keyboard cores unchanged. Make the nRF52 adapters self-contained and faithful to real Adafruit BSP APIs, use `BLEViaTransport` directly as the protocol transport, and keep all hardware diagnostics outside production firmware under `tests/hardware/`. GitHub Actions is the authoritative compiler and produces the exact UF2 artifacts used for physical acceptance.

**Tech Stack:** C++11, Arduino CLI, Adafruit nRF52 BSP `1.7.0`, Bluefruit52Lib, SoftDevice S140, InternalFS/LittleFS, FreeRTOS, Python stdlib, browser Web Bluetooth, GitHub Actions.

## Global Constraints

- Base design: `docs/superpowers/specs/2026-08-05-nice-nano-ci-hardware-acceptance-design.md`.
- Board: nice!nano v2 / Pro Micro-form-factor nRF52840.
- Arduino core: Adafruit nRF52 BSP `1.7.0`.
- Board variant: `somik123/Adafruit_nRF52_Arduino_ProMicro` commit `bd0fdcf124f59662d0184c39126e456f89dccd9c`.
- Compile FQBN: `adafruit:nrf52:promicronrf52840`.
- VIA protocol version: `0x000D`; packets are exactly 32 bytes.
- Production firmware is BLE-only; no TinyUSB Raw HID or USB keyboard path.
- Storage paths remain `/via_a.dat` and `/via_b.dat`.
- Storage staging is 4096 bytes; protocol load buffer is `sizeof(keymap) + sizeof(uint32_t)`.
- No split transport, BLE central mode, battery, deep sleep, RGB, encoder, DFU command, or board package.
- Do not modify portable protocol semantics or unrelated RP2040, STM32F103, or ESP32-S3 behavior.
- Do not add dependencies; use BSP APIs, standard library, Python stdlib, and browser APIs already available.
- Do not commit `nrf52_ble_transport_test`, `nrf52_ble_transport_test_san`, or other generated binaries.
- Local `g++` and `arduino-cli` are unavailable. GitHub Actions output is required before any pass claim.
- At execution start, `src/VIA_nRF52_InternalFS.cpp` and `tests/nrf52_internalfs_test.cpp` contain staged CRC work. Review and continue it; do not discard or recreate it.
- Support stays `experimental / compile-verified` until every physical acceptance check passes.

## File Map

- `src/VIA_nRF52_InternalFS.cpp`: self-contained CRC and dual-slot validation.
- `tests/fakes/InternalFileSystem.h`: mount, write-failure, and corruption controls.
- `tests/nrf52_internalfs_test.cpp`: CRC, mount, commit, fallback, and durability tests.
- `src/VIA_nRF52_BLE_ViaTransport.h/.cpp`: FF60 transport, FF62 metadata, and static mutex.
- `tests/fakes/bluefruit.h`, `tests/fakes/bluefruit.cpp`: real-signature Bluefruit and mutex fake.
- `tests/nrf52_ble_transport_test.cpp`: GATT, metadata, locking, and single-instance tests.
- `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino`: BLE-only 2x3 production reference.
- `tests/nice_nano_sketch_test.py`: static firmware architecture contract.
- `tests/hardware/nice_nano_ble_packet_test.html`: manual browser packet-test UI.
- `tests/hardware/nice_nano_ble_packet_test.js`: Web Bluetooth invalid/valid packet driver.
- `tests/hardware/nice_nano_storage_corruptor/nice_nano_storage_corruptor.ino`: one-shot latest-slot corruptor.
- `.github/workflows/ci.yml`: complete compile gates and hashed UF2 artifacts.
- `docs/nice-nano-v2-hardware-checklist.md`: physical evidence ledger.
- `README.md`, `CHANGELOG.md`: support status after physical acceptance.

---

### Task 1: Remove the CRC Link Dependency

**Files:**
- Modify: `src/VIA_nRF52_InternalFS.cpp:4-29`
- Modify: `tests/nrf52_internalfs_test.cpp:6-20`

**Interfaces:**
- Consumes: byte buffer, length, and optional previous CRC state.
- Produces: reflected CRC32 state using polynomial `0xEDB88320`, initial state `0xFFFFFFFF`, and no final XOR.

- [ ] **Step 1: Confirm the existing red evidence and staged scope**

Run:

```bash
gh run view 30991170305 --job 92257405637 --log-failed
git status --short
git diff --cached -- src/VIA_nRF52_InternalFS.cpp tests/nrf52_internalfs_test.cpp
```

Expected: GitHub log contains `undefined reference to crc32_compute`; only the two intended source/test files are staged, while generated native binaries remain untracked.

- [ ] **Step 2: Add a production-record CRC regression check**

Add this function to `tests/nrf52_internalfs_test.cpp` and call it from `main()` before `test_storage()`:

```cpp
void test_crc32_record() {
    InternalFS.format();
    g_fake_fs_write_fail = false;
    uint8_t staging[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    assert(storage.commit());

    struct Header { uint32_t magic, generation, length, crc; } header = {};
    const std::vector<uint8_t>& record = InternalFS.files_.at("/via_a.dat");
    assert(record.size() == sizeof(header) + sizeof(staging));
    memcpy(&header, record.data(), sizeof(header));
    assert(header.magic == 0x56494146);
    assert(header.generation == 1);
    assert(header.length == sizeof(staging));
    assert(header.crc == 0x340BC6D9);
}
```

This reads the CRC emitted by production storage code; it does not duplicate the implementation algorithm.

- [ ] **Step 3: Keep the minimal self-contained CRC implementation**

Ensure `src/VIA_nRF52_InternalFS.cpp` contains no declaration or call to `crc32_compute` and uses exactly:

```cpp
static uint32_t crc32(const uint8_t* data, size_t size,
                      uint32_t crc = 0xFFFFFFFF) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return crc;
}
```

Chunked record validation must call `crc32(chunk, toRead, fileCrc)`. Full staging commit must call `crc32(staging_, capacity_)`.

- [ ] **Step 4: Stage only the CRC change and inspect it**

Run:

```bash
git add src/VIA_nRF52_InternalFS.cpp tests/nrf52_internalfs_test.cpp
git diff --cached --check
git diff --cached --stat
git status --short
```

Expected: no whitespace errors; generated binaries remain untracked and unstaged.

- [ ] **Step 5: Commit and push the CRC fix**

```bash
git commit -m "fix(nrf52): remove external CRC dependency"
git push origin main
```

- [ ] **Step 6: Verify the authoritative CI result**

Run:

```bash
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: native InternalFS tests and the pinned nice!nano link step pass. If
any step fails, run this and stop before Task 2:

```bash
gh run view $(gh run list --limit 1 --json databaseId \
  --jq '.[0].databaseId') --log-failed
```

### Task 2: Verify Full InternalFS Writes

**Files:**
- Modify: `tests/fakes/InternalFileSystem.h:8-92`
- Modify: `tests/nrf52_internalfs_test.cpp`
- Modify: `src/VIA_nRF52_InternalFS.cpp:39-198`

**Interfaces:**
- Consumes: `InternalFS.begin()`, two record files, caller-owned staging buffer.
- Produces: `InternalFSStorage::begin()` that reports mount failure and `commit()` that verifies header plus full payload CRC before advancing generation.

- [ ] **Step 1: Add fake mount and post-write corruption controls**

At the top of `tests/fakes/InternalFileSystem.h`, declare:

```cpp
extern bool g_fake_fs_begin_ok;
extern bool g_fake_fs_write_fail;
extern bool g_fake_fs_corrupt_after_write;
```

Change `LittleFS::begin()` to:

```cpp
bool begin() { return g_fake_fs_begin_ok; }
```

Change `File::close()` to corrupt the just-written record only when requested:

```cpp
void close() {
    if (data_ && mode_ == FILE_O_WRITE &&
        ::g_fake_fs_corrupt_after_write && !data_->empty()) {
        data_->back() ^= 0xFF;
        ::g_fake_fs_corrupt_after_write = false;
    }
    data_ = nullptr;
    error_ = true;
}
```

- [ ] **Step 2: Write failing mount and payload-verification tests**

Define the new globals in `tests/nrf52_internalfs_test.cpp`:

```cpp
bool g_fake_fs_begin_ok = true;
bool g_fake_fs_write_fail = false;
bool g_fake_fs_corrupt_after_write = false;
```

Add and call these tests:

```cpp
void test_mount_failure() {
    uint8_t staging[32] = {};
    g_fake_fs_begin_ok = false;
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(!storage.begin());
    g_fake_fs_begin_ok = true;
}

void test_corrupt_write_preserves_previous_slot() {
    InternalFS.format();
    uint8_t staging[32] = {};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    staging[0] = 0x11;
    assert(storage.commit());

    staging[0] = 0x22;
    g_fake_fs_corrupt_after_write = true;
    assert(!storage.commit());

    uint8_t recovered[32] = {};
    via::nrf52::InternalFSStorage restarted(recovered, sizeof(recovered));
    assert(restarted.begin());
    assert(recovered[0] == 0x11);
}
```

- [ ] **Step 3: Run the focused test to verify red**

Run in an environment with `g++`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 \
  -DNRF52840_XXAA -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test && \
  ./nrf52_internalfs_test
```

Expected: FAIL because mount failure is ignored and commit validates only the header. If local `g++` is still unavailable, push only after Steps 4-6 and use the `nrf52-adapters` CI job as the red/green runner.

- [ ] **Step 4: Return false on mount failure**

At the start of `InternalFSStorage::begin()` use:

```cpp
if (!InternalFS.begin()) return false;
```

Do not call `format()`.

- [ ] **Step 5: Verify the complete newly written payload**

After validating `readHeader` in `commit()`, replace the existing header-only close with:

```cpp
uint32_t payloadCrc = 0xFFFFFFFF;
uint32_t remaining = readHeader.length;
while (remaining > 0) {
  uint8_t chunk[256];
  const size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
  if (file.read(chunk, toRead) != toRead) {
    file.close();
    return false;
  }
  payloadCrc = crc32(chunk, toRead, payloadCrc);
  remaining -= toRead;
}
file.close();
if (payloadCrc != readHeader.crc32) return false;

activeGeneration_ = nextGen;
return true;
```

Delete the stale comment claiming header validation is enough.

- [ ] **Step 6: Run normal and sanitizer tests**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 \
  -DNRF52840_XXAA -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test && \
  ./nrf52_internalfs_test
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -Itests/fakes -Isrc tests/nrf52_internalfs_test.cpp \
  src/VIA_nRF52_InternalFS.cpp -o nrf52_internalfs_test_san && \
  ./nrf52_internalfs_test_san
```

Expected: both print `PASS` with no sanitizer output.

- [ ] **Step 7: Commit, push, and verify CI**

```bash
git add src/VIA_nRF52_InternalFS.cpp tests/fakes/InternalFileSystem.h \
  tests/nrf52_internalfs_test.cpp
git diff --cached --check
git commit -m "fix(nrf52): verify InternalFS payload writes"
git push origin main
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: complete CI succeeds before Task 3.

### Task 3: Complete the Bluefruit Transport Contract

**Files:**
- Modify: `src/VIA_nRF52_BLE_ViaTransport.h`
- Modify: `src/VIA_nRF52_BLE_ViaTransport.cpp`
- Modify: `tests/fakes/bluefruit.h`
- Modify: `tests/fakes/bluefruit.cpp`
- Modify: `tests/nrf52_ble_transport_test.cpp`

**Interfaces:**
- Consumes: real `BLECharacteristic::write_cb_t`, caller-initialized Bluefruit, FreeRTOS static mutex.
- Produces: single-instance `BLEViaTransport`, complete FF62 payload, lock-safe request queue, and `BLEService& service()`.

- [ ] **Step 1: Make the fake expose service metadata and mutex failure**

In the test-only branch of `VIA_nRF52_BLE_ViaTransport.h`, replace the current mutex fake with:

```cpp
struct StaticSemaphore_t {};
typedef void* SemaphoreHandle_t;
extern bool g_fake_mutex_take;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
  return storage;
}
inline bool xSemaphoreTake(SemaphoreHandle_t, int) { return g_fake_mutex_take; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
```

Extend `FakeBluefruit` with FF61/FF62 properties, permissions, and fixed length:

```cpp
static uint8_t ff61Properties;
static uint8_t ff61ReadPermission;
static uint8_t ff61WritePermission;
static uint8_t ff62Properties;
static uint8_t ff62ReadPermission;
static uint8_t ff62WritePermission;
static uint16_t ff62FixedLength;
```

Define their storage beside the existing fake statics in
`tests/nrf52_ble_transport_test.cpp`:

```cpp
uint8_t FakeBluefruit::ff61Properties = 0;
uint8_t FakeBluefruit::ff61ReadPermission = 0;
uint8_t FakeBluefruit::ff61WritePermission = 0;
uint8_t FakeBluefruit::ff62Properties = 0;
uint8_t FakeBluefruit::ff62ReadPermission = 0;
uint8_t FakeBluefruit::ff62WritePermission = 0;
uint16_t FakeBluefruit::ff62FixedLength = 0;
```

Populate these fields from `BLECharacteristic::begin()` in
`tests/fakes/bluefruit.cpp`:

```cpp
void BLECharacteristic::begin() {
    if (uuid == 0xFF61) {
        FakeBluefruit::ff61Uuid = uuid;
        FakeBluefruit::ff61Properties = props;
        FakeBluefruit::ff61ReadPermission = readPerm;
        FakeBluefruit::ff61WritePermission = writePerm;
        FakeBluefruit::ff61FixedLength = fixedLen;
        FakeBluefruit::ff61Cb = writeCb;
    } else if (uuid == 0xFF62) {
        FakeBluefruit::ff62Uuid = uuid;
        FakeBluefruit::ff62Properties = props;
        FakeBluefruit::ff62ReadPermission = readPerm;
        FakeBluefruit::ff62WritePermission = writePerm;
        FakeBluefruit::ff62FixedLength = fixedLen;
    }
}
```

Add these assignments to `FakeBluefruit::reset()`:

```cpp
ff61Properties = 0;
ff61ReadPermission = 0;
ff61WritePermission = 0;
ff62Properties = 0;
ff62ReadPermission = 0;
ff62WritePermission = 0;
ff62FixedLength = 0;
```

Change `dispatchWrite()` so every length reaches the production callback:

```cpp
static bool dispatchWrite(const uint8_t* data, uint16_t len) {
  if (!ff61Cb) return false;
  ff61Cb(0, nullptr, const_cast<uint8_t*>(data), len);
  return true;
}
```

- [ ] **Step 2: Add failing FF62, lock, and single-instance tests**

Define `bool g_fake_mutex_take = true;` in `tests/nrf52_ble_transport_test.cpp`. Add assertions to `test_initialization()`:

```cpp
assert(FakeBluefruit::ff61Properties ==
       (CHR_PROPS_READ | CHR_PROPS_WRITE |
        CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY));
assert(FakeBluefruit::ff61ReadPermission == SECMODE_OPEN);
assert(FakeBluefruit::ff61WritePermission == SECMODE_OPEN);
assert(FakeBluefruit::ff62Properties == CHR_PROPS_READ);
assert(FakeBluefruit::ff62ReadPermission == SECMODE_OPEN);
assert(FakeBluefruit::ff62WritePermission == SECMODE_NO_ACCESS);
assert(FakeBluefruit::ff62FixedLength == via::kPacketSize);
assert(memcmp(FakeBluefruit::ff62Value + 4,
              "1234567890123456789012345678", 28) == 0);
```

Add and call:

```cpp
void test_short_name_zero_padding() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    assert(memcmp(FakeBluefruit::ff62Value + 4, "AirVIA", 6) == 0);
    for (size_t i = 10; i < 32; ++i) assert(FakeBluefruit::ff62Value[i] == 0);
}

void test_lock_failure_drops_packet() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    uint8_t packet[32] = {42};
    uint8_t received[32] = {};
    g_fake_mutex_take = false;
    assert(FakeBluefruit::dispatchWrite(packet, sizeof(packet)));
    g_fake_mutex_take = true;
    assert(!transport.receive(received));
}

void test_rejects_second_live_instance() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport first;
    via::nrf52::BLEViaTransport second;
    assert(first.begin("AirVIA", 1));
    assert(!second.begin("AirVIA", 1));
}
```

- [ ] **Step 3: Run the focused test to verify red**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 \
  -DNRF52840_XXAA -DTESTING_ENVIRONMENT -Itests/fakes -Isrc \
  tests/nrf52_ble_transport_test.cpp src/VIA_nRF52_BLE_ViaTransport.cpp \
  tests/fakes/bluefruit.cpp -o nrf52_ble_transport_test && \
  ./nrf52_ble_transport_test
```

Expected: FAIL because FF62 omits the name, mutex acquisition is ignored, and a second instance is accepted.

- [ ] **Step 4: Use a static mutex and explicit lifetime**

Add to the public/private class definition:

```cpp
~BLEViaTransport() override;
StaticSemaphore_t mutexStorage_;
SemaphoreHandle_t mutex_;
```

Initialize the handle with `xSemaphoreCreateMutexStatic(&mutexStorage_)`. Add:

```cpp
BLEViaTransport::~BLEViaTransport() {
  if (activeTransport_ == this) activeTransport_ = nullptr;
}
```

At the start of `begin()`:

```cpp
if (!deviceName || !mutex_ || activeTransport_) return false;
activeTransport_ = this;
```

- [ ] **Step 5: Populate all FF62 bytes**

After writing version bytes, add:

```cpp
size_t nameLength = strlen(deviceName);
if (nameLength > 28) nameLength = 28;
memcpy(info + 4, deviceName, nameLength);
```

The existing zero-initialization supplies padding.

- [ ] **Step 6: Honor every mutex acquisition result**

Implement callback and receive entry as:

```cpp
if (!activeTransport_ || len != kPacketSize) return;
if (!xSemaphoreTake(activeTransport_->mutex_, 0)) return;
```

and:

```cpp
if (!xSemaphoreTake(mutex_, 10)) return false;
```

Only call `xSemaphoreGive()` after a successful take. Keep first-packet preservation and increment `droppedPackets_` only while holding the mutex.

- [ ] **Step 7: Run normal and sanitizer transport tests**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -DARDUINO_ARCH_NRF52 \
  -DNRF52840_XXAA -DTESTING_ENVIRONMENT -Itests/fakes -Isrc \
  tests/nrf52_ble_transport_test.cpp src/VIA_nRF52_BLE_ViaTransport.cpp \
  tests/fakes/bluefruit.cpp -o nrf52_ble_transport_test && \
  ./nrf52_ble_transport_test
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -DARDUINO_ARCH_NRF52 -DNRF52840_XXAA \
  -DTESTING_ENVIRONMENT -Itests/fakes -Isrc \
  tests/nrf52_ble_transport_test.cpp src/VIA_nRF52_BLE_ViaTransport.cpp \
  tests/fakes/bluefruit.cpp -o nrf52_ble_transport_test_san && \
  ./nrf52_ble_transport_test_san
```

Expected: both print `All tests passed!` with no warnings or sanitizer output.

- [ ] **Step 8: Commit, push, and verify CI**

```bash
git add src/VIA_nRF52_BLE_ViaTransport.h src/VIA_nRF52_BLE_ViaTransport.cpp \
  tests/fakes/bluefruit.h tests/fakes/bluefruit.cpp \
  tests/nrf52_ble_transport_test.cpp
git diff --cached --check
git commit -m "fix(nrf52): complete Bluefruit transport contract"
git push origin main
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: full CI succeeds before Task 4.

### Task 4: Restore the BLE-Only Reference Sketch

**Files:**
- Create: `tests/nice_nano_sketch_test.py`
- Modify: `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino`
- Modify: `.github/workflows/ci.yml:107-132`

**Interfaces:**
- Consumes: `MatrixIOArduino`, `BLEKeyboardHID`, `BLEViaTransport`, `InternalFSStorage`.
- Produces: one BLE-only 2x3 firmware where `via::Protocol` polls `BLEViaTransport` directly.

- [ ] **Step 1: Write the static sketch contract test**

Create `tests/nice_nano_sketch_test.py`:

```python
from pathlib import Path

source = Path(
    "examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino"
).read_text(encoding="utf-8")

for forbidden in (
    "Adafruit_TinyUSB.h",
    "VIA_TinyUSB_RawHID.h",
    "VIA_TinyUSB_Keyboard.h",
    "viaRawHid",
    "usbKeyboard",
    "blePacket",
    "protocol.process(",
):
    assert forbidden not in source, forbidden

for required in (
    "static uint8_t storageStaging[4096]",
    "via::Protocol protocol(protocolConfig, bleVia, &storage);",
    "Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);",
    "Bluefruit.begin(1, 0);",
    "Bluefruit.Security.setIOCaps(false, false, false);",
    "bleHidSvc.begin();",
    "bleHid.begin();",
    'bleVia.begin("AirVIA nice!nano", 0x00000001);',
    "storage.begin();",
    "protocol.begin(millis());",
    "keyboard.begin();",
    "startAdvertising();",
):
    assert required in source, required

order = (
    "Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);",
    "Bluefruit.begin(1, 0);",
    "bleHidSvc.begin();",
    "bleHid.begin();",
    'bleVia.begin("AirVIA nice!nano", 0x00000001);',
    "storage.begin();",
    "protocol.begin(millis());",
    "keyboard.begin();",
    "startAdvertising();",
)
positions = [source.index(item) for item in order]
assert positions == sorted(positions)
```

- [ ] **Step 2: Run the contract test to verify red**

```bash
python tests/nice_nano_sketch_test.py
```

Expected: FAIL on TinyUSB references, 128-byte staging, wrong protocol transport, and initialization order.

- [ ] **Step 3: Remove unused and USB-only includes**

Keep only:

```cpp
#include <Arduino.h>
#include <bluefruit.h>
#include <VIA_Arduino.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_nRF52_GPIO.h>
#include <VIA_nRF52_InternalFS.h>
#include <VIA_nRF52_BLE.h>
#include <VIA_nRF52_BLE_ViaTransport.h>
```

- [ ] **Step 4: Reorder globals around the direct BLE transport**

Use:

```cpp
static uint16_t keymap[LAYERS * ROWS * COLS] = {};
static uint8_t loadBuffer[sizeof(keymap) + sizeof(uint32_t)] = {};
static uint8_t storageStaging[4096] = {};

via::nrf52::InternalFSStorage storage(storageStaging, sizeof(storageStaging));
BLEHidAdafruit bleHidSvc;
via::nrf52::BLEKeyboardHID bleHid(bleHidSvc);
via::nrf52::BLEViaTransport bleVia;
via::Protocol protocol(protocolConfig, bleVia, &storage);
```

Delete `BLEDis`, `RawHID`, and USB keyboard objects.

- [ ] **Step 5: Replace setup and loop with one-owner initialization**

Use:

```cpp
void setup() {
    Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
    if (!Bluefruit.begin(1, 0)) return;
    Bluefruit.setName("AirVIA nice!nano");
    Bluefruit.setTxPower(4);
    Bluefruit.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    Bluefruit.Security.setIOCaps(false, false, false);

    bleHidSvc.begin();
    if (!bleHid.begin()) return;
    if (!bleVia.begin("AirVIA nice!nano", 0x00000001)) return;
    if (!storage.begin()) return;
    if (!protocol.begin(millis())) return;
    if (!keyboard.begin()) return;
    startAdvertising();
}

void loop() {
    const uint32_t now = millis();
    protocol.task(now);
    keyboard.task(now);
}
```

Keep advertising both `bleHidSvc` and `bleVia.service()` with indefinite duration.

- [ ] **Step 6: Add static sketch and JSON checks to CI**

Add before native adapter compilation in the `nrf52-adapters` job:

```yaml
      - name: Validate nice!nano example contracts
        run: |
          python tests/nice_nano_definition_test.py
          python tests/nice_nano_sketch_test.py
```

- [ ] **Step 7: Run static checks**

```bash
python tests/nice_nano_definition_test.py
python tests/nice_nano_sketch_test.py
git diff --check
```

Expected: both scripts exit zero and `git diff --check` is silent.

- [ ] **Step 8: Commit, push, and verify the real board compile**

```bash
git add examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino \
  tests/nice_nano_sketch_test.py .github/workflows/ci.yml
git commit -m "fix(nrf52): restore BLE-only nice nano example"
git push origin main
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: static contracts and `adafruit:nrf52:promicronrf52840` compile pass.

### Task 5: Add Hardware Acceptance Utilities

**Files:**
- Create: `tests/hardware/nice_nano_ble_packet_test.html`
- Create: `tests/hardware/nice_nano_ble_packet_test.js`
- Create: `tests/hardware/nice_nano_storage_corruptor/nice_nano_storage_corruptor.ino`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: browser utility for FF61 length checks and one-shot sketch that corrupts only the newest InternalFS slot.
- Consumes: FF60/FF61 Web Bluetooth service and existing `/via_a.dat`/`/via_b.dat` records.

- [ ] **Step 1: Create the minimal browser page**

Create `tests/hardware/nice_nano_ble_packet_test.html`:

```html
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>nice!nano FF61 packet test</title>
<body>
  <h1>nice!nano FF61 packet test</h1>
  <button id="run">Connect and test</button>
  <pre id="log"></pre>
  <script src="nice_nano_ble_packet_test.js"></script>
</body>
</html>
```

- [ ] **Step 2: Create the Web Bluetooth packet driver**

Create `tests/hardware/nice_nano_ble_packet_test.js`:

```javascript
const SERVICE = "0000ff60-0000-1000-8000-00805f9b34fb";
const FF61 = "0000ff61-0000-1000-8000-00805f9b34fb";
const output = document.querySelector("#log");
const log = (message) => { output.textContent += `${message}\n`; };
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function write(characteristic, length) {
  const packet = new Uint8Array(length);
  packet[0] = 0x01;
  try {
    await characteristic.writeValueWithResponse(packet);
    log(`${length}-byte write accepted by GATT`);
  } catch (error) {
    log(`${length}-byte write rejected: ${error.name}`);
  }
}

document.querySelector("#run").addEventListener("click", async () => {
  output.textContent = "";
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{services: [SERVICE]}],
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    const ff61 = await service.getCharacteristic(FF61);

    await write(ff61, 31);
    await write(ff61, 33);
    await write(ff61, 32);

    let validResponse = false;
    for (let attempt = 0; attempt < 20; attempt += 1) {
      await sleep(50);
      const response = await ff61.readValue();
      if (response.byteLength === 32 && response.getUint8(2) === 0x0d) {
        validResponse = true;
        break;
      }
    }
    if (!validResponse) throw new Error("valid VIA protocol response missing");
    log("PASS: valid 32-byte traffic works after invalid writes");
  } catch (error) {
    log(`FAIL: ${error.message}`);
  }
});
```

- [ ] **Step 3: Syntax-check the browser driver**

```bash
node --check tests/hardware/nice_nano_ble_packet_test.js
```

Expected: no output and exit zero.

- [ ] **Step 4: Create the one-shot storage corruptor**

Create `tests/hardware/nice_nano_storage_corruptor/nice_nano_storage_corruptor.ino`:

```cpp
#include <Arduino.h>
#include <InternalFileSystem.h>

struct RecordHeader {
  uint32_t magic;
  uint32_t generation;
  uint32_t length;
  uint32_t crc32;
};

static bool readHeader(const char* path, RecordHeader& header) {
  Adafruit_LittleFS_Namespace::File file =
      InternalFS.open(path, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (!file) return false;
  const bool ok = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) ==
                  sizeof(header);
  file.close();
  return ok && header.magic == 0x56494146 && header.length == 4096;
}

static bool newer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  if (!InternalFS.begin()) {
    Serial.println("FAIL: mount");
    return;
  }

  RecordHeader a = {}, b = {};
  if (!readHeader("/via_a.dat", a) || !readHeader("/via_b.dat", b)) {
    Serial.println("FAIL: two committed slots required");
    return;
  }

  const char* newest = newer(a.generation, b.generation)
                           ? "/via_a.dat" : "/via_b.dat";
  Adafruit_LittleFS_Namespace::File file =
      InternalFS.open(newest, Adafruit_LittleFS_Namespace::FILE_O_WRITE);
  if (!file) {
    Serial.println("FAIL: open newest slot");
    return;
  }
  RecordHeader invalid = {};
  const bool ok = file.write(reinterpret_cast<const uint8_t*>(&invalid),
                             sizeof(invalid)) == sizeof(invalid);
  file.close();
  Serial.println(ok ? "PASS: newest slot corrupted" : "FAIL: write");
}

void loop() {}
```

- [ ] **Step 5: Add utility checks to CI**

Add JavaScript syntax validation to `nrf52-adapters`:

```yaml
      - name: Validate hardware utility JavaScript
        run: node --check tests/hardware/nice_nano_ble_packet_test.js
```

After compiling the production sketch in the `test` job, compile the corruptor:

```yaml
      - name: Compile nice!nano storage corruptor
        run: |
          arduino-cli compile --fqbn adafruit:nrf52:promicronrf52840 \
            --libraries . tests/hardware/nice_nano_storage_corruptor
```

- [ ] **Step 6: Commit, push, and verify both utility gates**

```bash
git add tests/hardware .github/workflows/ci.yml
git diff --cached --check
git commit -m "test(nrf52): add hardware acceptance utilities"
git push origin main
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: JavaScript syntax and corruptor compile pass with the full CI suite.

### Task 6: Publish Pinned UF2 Artifacts

**Files:**
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: production and corruptor HEX files compiled by Adafruit BSP `1.7.0`.
- Produces: `nice_nano_v2_VIA_BLE.uf2`, `nice_nano_storage_corruptor.uf2`, and `SHA256SUMS.txt` in one GitHub Actions artifact.

- [ ] **Step 1: Compile both sketches into explicit output directories**

Replace the two nice!nano compile steps with:

```yaml
      - name: Compile nice!nano firmware and acceptance corruptor
        run: |
          mkdir -p build/nice_nano build/corruptor
          arduino-cli compile --fqbn adafruit:nrf52:promicronrf52840 \
            --libraries . --output-dir build/nice_nano \
            examples/nice_nano_v2_VIA_BLE
          arduino-cli compile --fqbn adafruit:nrf52:promicronrf52840 \
            --libraries . --output-dir build/corruptor \
            tests/hardware/nice_nano_storage_corruptor
```

- [ ] **Step 2: Convert the exact HEX files to UF2 and hash them**

Add immediately after compilation:

```yaml
      - name: Build UF2 acceptance artifacts
        run: |
          CORE="$HOME/.arduino15/packages/adafruit/hardware/nrf52/1.7.0"
          FIRMWARE_HEX=$(find build/nice_nano -maxdepth 1 -name '*.hex' -print -quit)
          CORRUPTOR_HEX=$(find build/corruptor -maxdepth 1 -name '*.hex' -print -quit)
          test -n "$FIRMWARE_HEX"
          test -n "$CORRUPTOR_HEX"
          python "$CORE/tools/uf2conv/uf2conv.py" -f 0xADA52840 -c \
            -o build/nice_nano_v2_VIA_BLE.uf2 "$FIRMWARE_HEX"
          python "$CORE/tools/uf2conv/uf2conv.py" -f 0xADA52840 -c \
            -o build/nice_nano_storage_corruptor.uf2 "$CORRUPTOR_HEX"
          sha256sum build/*.uf2 > build/SHA256SUMS.txt
```

- [ ] **Step 3: Upload one traceable artifact**

Add:

```yaml
      - name: Upload nice!nano acceptance artifacts
        uses: actions/upload-artifact@v4
        with:
          name: nice-nano-v2-${{ github.sha }}
          path: |
            build/nice_nano_v2_VIA_BLE.uf2
            build/nice_nano_storage_corruptor.uf2
            build/SHA256SUMS.txt
          if-no-files-found: error
          retention-days: 30
```

- [ ] **Step 4: Commit and push artifact publication**

```bash
git add .github/workflows/ci.yml
git diff --cached --check
git commit -m "ci(nrf52): publish pinned nice nano UF2"
git push origin main
```

- [ ] **Step 5: Verify the workflow and artifact contents**

```bash
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
gh run download $(gh run list --limit 1 --json databaseId \
  --jq '.[0].databaseId') -n "nice-nano-v2-$(git rev-parse HEAD)" \
  -D artifacts/nice-nano-v2
```

On Windows PowerShell, compare generated hashes:

```powershell
Get-FileHash -Algorithm SHA256 "artifacts\nice-nano-v2\nice_nano_v2_VIA_BLE.uf2"
Get-FileHash -Algorithm SHA256 "artifacts\nice-nano-v2\nice_nano_storage_corruptor.uf2"
Get-Content "artifacts\nice-nano-v2\SHA256SUMS.txt"
```

Expected: CI succeeds, both UF2 files exist, and hashes match `SHA256SUMS.txt`.

### Task 7: Record Compile Acceptance

**Files:**
- Modify: `docs/nice-nano-v2-hardware-checklist.md`

**Interfaces:**
- Consumes: green Actions run and downloaded artifact hashes.
- Produces: immutable compile evidence used by physical acceptance.

- [ ] **Step 1: Replace the minimal checklist header with evidence fields**

Use:

```markdown
# nice!nano v2 Hardware Verification Checklist

Status: Pending physical hardware verification

## Build Evidence

- Date:
- Commit SHA:
- GitHub Actions run:
- FQBN: `adafruit:nrf52:promicronrf52840`
- Adafruit nRF52 BSP: `1.7.0`
- Variant commit: `bd0fdcf124f59662d0184c39126e456f89dccd9c`
- Firmware SHA-256:
- Corruptor SHA-256:

## Fixture

- Board: nice!nano v2
- Matrix: 2x3, rows D0/D1, columns D2/D3/D4
- Host OS:
- Browser:
```

Before committing, populate every blank from these commands and the browser's
About page. Do not invent values:

```powershell
Get-Date -Format yyyy-MM-dd
git rev-parse HEAD
gh run view $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') `
  --json url --jq '.url'
(Get-CimInstance Win32_OperatingSystem).Caption
Get-FileHash -Algorithm SHA256 `
  "artifacts\nice-nano-v2\nice_nano_v2_VIA_BLE.uf2"
Get-FileHash -Algorithm SHA256 `
  "artifacts\nice-nano-v2\nice_nano_storage_corruptor.uf2"
```

- [ ] **Step 2: Add explicit unchecked acceptance sections**

Append:

```markdown
## Flash and BLE HID

- [ ] UF2 hash verified before flash.
- [ ] Production UF2 flashed through nice!nano bootloader.
- [ ] Device advertises as `AirVIA nice!nano`.
- [ ] Layer 0 types A/B/C/D/E; MO(1) emits no literal key.
- [ ] MO(1) plus first five switches types 1/2/3/4/5.
- [ ] No stuck key after release or reconnect.

## AirVIA

- [ ] FF60 service discovered.
- [ ] VIA protocol version is `0x000D`.
- [ ] All 12 keymap entries synchronize.
- [ ] Remap acknowledgment received and typing changes immediately.

## Persistence and Recovery

- [ ] Remap survives BLE disconnect/reconnect.
- [ ] Remap survives reset.
- [ ] Remap survives at least ten seconds without power.
- [ ] HID and AirVIA recover after deleting and recreating the OS bond.
- [ ] 31-byte and 33-byte FF61 writes do not damage valid traffic or keymap.
- [ ] Corrupting newest slot restores the older committed keymap.

## Notes

- Record each failure with exact symptom before any retry.
```

- [ ] **Step 3: Commit compile evidence without checking hardware boxes**

```bash
git add docs/nice-nano-v2-hardware-checklist.md \
  docs/superpowers/specs/2026-08-05-nice-nano-ci-hardware-acceptance-design.md \
  docs/superpowers/plans/2026-08-05-nice-nano-ci-hardware-acceptance.md
git diff --cached --check
git commit -m "docs(nrf52): record compile acceptance evidence"
git push origin main
```

Expected: all physical boxes remain unchecked.

### Task 8: Complete Physical Hardware Acceptance

**Files:**
- Modify: `docs/nice-nano-v2-hardware-checklist.md`
- Modify: `README.md:20-23,201-225`
- Modify: `CHANGELOG.md:3-9`

**Interfaces:**
- Consumes: hashed production/corruptor UF2 files, nice!nano v2, wired 2x3 matrix, Chrome/Edge, AirVIA.
- Produces: physical evidence and narrowly worded hardware-verified status.

- [ ] **Step 1: Inspect the fixture before power**

Verify with continuity mode:

```text
Rows: D0, D1
Columns: D2, D3, D4
Each switch closes exactly one row-column pair.
No row or column is shorted to VCC or ground.
```

Record any wiring correction in checklist notes.

- [ ] **Step 2: Verify and flash the production UF2**

On Windows PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 "artifacts\nice-nano-v2\nice_nano_v2_VIA_BLE.uf2"
```

Compare with `SHA256SUMS.txt`. Double-tap reset, wait for the `NICENANO` drive, and copy `nice_nano_v2_VIA_BLE.uf2` to it. Check the first three flash/advertising boxes only after normal reboot and the exact BLE name appears.

- [ ] **Step 3: Verify BLE HID layers**

Pair through OS Bluetooth settings. In a plain text editor:

```text
Layer 0 expected: A B C / D E [MO(1), no output]
Hold MO(1):       1 2 3 / 4 5 [transparent]
```

Release every switch, disconnect/reconnect, and press A once. Check HID boxes only if output contains no stuck or repeated key.

- [ ] **Step 4: Verify AirVIA synchronization and first persisted state**

Open `https://juarendra.github.io/AirVIA/` in Chrome or Edge, load `examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.json`, and connect the device.

Confirm protocol `0x000D` and 12 keymap entries. Remap physical A to F, wait at least two seconds for the 750 ms autosave, reset, and verify F. This creates the first valid slot.

- [ ] **Step 5: Verify disconnect, power-cycle, and bond recovery**

1. Disconnect and reconnect AirVIA; verify F.
2. Reset board; verify F.
3. Remove USB power for at least ten seconds; reconnect and verify F.
4. Delete OS BLE bond, pair again, reconnect AirVIA, and verify F.

Record exact behavior for each independent check.

- [ ] **Step 6: Run invalid packet acceptance**

Serve the hardware utility from repository root:

```powershell
python -m http.server 8000
```

Open `http://localhost:8000/tests/hardware/nice_nano_ble_packet_test.html`, click **Connect and test**, and require:

```text
PASS: valid 32-byte traffic works after invalid writes
```

Reset the board, reconnect AirVIA, and verify F still exists before checking the invalid-packet box.

- [ ] **Step 7: Create a distinguishable second storage slot**

In AirVIA, remap the same physical key from F to G. Wait two seconds, reset, and verify G. Expected durable states are now:

```text
Older slot: F
Newest slot: G
```

- [ ] **Step 8: Corrupt only the newest slot**

Verify and flash `nice_nano_storage_corruptor.uf2`. Open serial monitor at 115200 and require:

```text
PASS: newest slot corrupted
```

If it prints `FAIL: two committed slots required`, reflash production firmware and repeat Steps 4 and 7; do not format InternalFS.

- [ ] **Step 9: Verify fallback to the older slot**

Reflash the original, already-hashed `nice_nano_v2_VIA_BLE.uf2` without erasing flash. Pair/reconnect, then require the physical key to type F, not G and not default A. Connect AirVIA and confirm valid protocol/keymap operation.

Check corrupt-slot recovery only after this exact result.

- [ ] **Step 10: Update status only if every box passes**

Change checklist status to:

```markdown
Status: Hardware-verified on one nice!nano v2 fixture
```

Change README support text to:

```markdown
- nRF52840 (nice!nano v2) compilation with Adafruit Bluefruit (AirVIA BLE
  transport). Hardware-verified on one nice!nano v2 2x3 fixture; experimental.
```

Under the README nice!nano heading, add:

```markdown
Status: **Experimental; hardware-verified on one nice!nano v2 2x3 fixture.**
See `docs/nice-nano-v2-hardware-checklist.md` for exact build and test evidence.
```

Under `CHANGELOG.md` Unreleased, add:

```markdown
### Verified
- nice!nano v2 BLE HID, AirVIA sync/remap, reset and power-cycle persistence,
  reconnect, invalid-packet rejection, and corrupt-newest-slot fallback on one
  2x3 hardware fixture. Support remains experimental.
```

If any box fails, leave README and changelog status unchanged.

- [ ] **Step 11: Commit and push acceptance evidence**

```bash
git add docs/nice-nano-v2-hardware-checklist.md README.md CHANGELOG.md
git diff --cached --check
git commit -m "docs(nrf52): record nice nano hardware acceptance"
git push origin main
```

- [ ] **Step 12: Verify final repository state and CI**

```bash
git status --short --branch
git log --oneline -10
gh run list --limit 1
gh run watch $(gh run list --limit 1 --json databaseId --jq '.[0].databaseId') \
  --exit-status
```

Expected: branch matches `origin/main`; only the two pre-existing generated binaries may remain untracked; final CI is green.

## Final Acceptance

- Self-contained CRC removes `crc32_compute` linker dependency.
- InternalFS mount and complete post-write payload verification are tested.
- Bluefruit fake matches production callback shape and metadata contract.
- FF62 contains big-endian version and 28-byte name field.
- Request queue uses a static mutex and honors lock failure.
- Reference sketch is BLE-only and uses `BLEViaTransport` directly.
- Production and corruptor UF2 files come from the pinned green CI run and have recorded SHA-256 hashes.
- BLE HID, AirVIA, persistence, reconnect, invalid packets, and corrupt-slot fallback pass on the physical 2x3 fixture.
- Documentation claims only one-fixture experimental hardware verification.
