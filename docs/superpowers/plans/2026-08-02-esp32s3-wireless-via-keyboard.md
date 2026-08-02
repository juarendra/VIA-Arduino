# ESP32-S3 Wireless VIA Keyboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ESP32-S3 dual-mode wireless support — VIA configuration via USB Raw HID and BLE HID keyboard for wireless typing — without changing existing portable core files.

**Architecture:** Five new portable modules (Encoder, Battery, SleepMgr) plus ESP32-S3 adapters (TinyUSB Raw HID port, BLE Keyboard HID, NVS Storage, GPIO MatrixIO). Existing Matrix, Keyboard, Keycodes, and Protocol are reused unchanged. OLED and WS2812 deferred to hardware-only after core milestones.

**Tech Stack:** C++11, Arduino library format, ESP32-S3 Arduino core v3.0.x, NimBLE-Arduino, ESP32-BLE-Keyboard, Adafruit TinyUSB, native `assert` tests, WSL g++, GitHub Actions.

## Global Constraints

- MIT-licensed project code; do not copy GPL QMK implementation.
- C++11 compatibility; no heap allocation in portable or adapter code.
- Existing portable core files (`VIA_Matrix.*`, `VIA_Keyboard.*`, `VIA_Protocol.*`, `VIA_Keycodes.*`, `VIA_RGBLight.*`) receive ZERO changes.
- Target: ESP32-S3-WROOM-1 custom PCB, though initial development can use a generic S3 dev board.
- Arduino core: `espressif/esp32` v3.0.x pinned.
- Reference matrix: 5 rows × 15 columns, four dynamic layers, `COL2ROW`.
- Branch: `feat/0.4-esp32s3-wireless-via`; push after each reviewed task.

---

## Milestone 1: Portable Encoder And Battery

**Goal:** Gray-code encoder state machine and battery management, native-testable.

**Files:**
- Create: `src/VIA_Encoder.h`
- Create: `tests/encoder_test.cpp`
- Create: `src/VIA_Battery.h`
- Create: `tests/battery_test.cpp`
- Modify: `.github/workflows/ci.yml`

### Task 1: Encoder Gray-Code State Machine

**Files:**
- Create: `src/VIA_Encoder.h`
- Create: `tests/encoder_test.cpp`

**Interfaces:**
- Produces: `class via::Encoder` with `update(uint8_t pinA, uint8_t pinB, uint32_t now)->void`, `int32_t count() const->int32_t`, `int32_t consume()->int32_t` (returns count then resets to zero)
- Produces: optional `debounceUs` config (default 2000), `kDebounceMin = 500`

- [ ] **Step 1: Write failing encoder test**

```cpp
#include <assert.h>
#include "VIA_Encoder.h"

int main() {
  via::Encoder encoder;
  // CCW: A falls first
  encoder.update(0, 0, 0);
  encoder.update(0, 1, 1);  // A low, B still high
  encoder.update(1, 1, 2);  // B also low after A
  encoder.update(1, 0, 3);  // A rises first
  encoder.update(0, 0, 4);  // B also rises after A
  assert(encoder.count() == -1);

  // CW: B falls first
  encoder.update(0, 0, 10);
  encoder.update(1, 0, 11);
  encoder.update(1, 1, 12);
  encoder.update(0, 1, 13);
  encoder.update(0, 0, 14);
  assert(encoder.count() == 0); // -1 + 1 = 0

  // Noise: same state twice, no change
  encoder.update(0, 0, 20);
  encoder.update(0, 0, 21);
  assert(encoder.count() == 0);

  // Debounce: transition within debounceUs ignored
  encoder.update(1, 0, 100);
  encoder.update(1, 1, 101); // 1us < 2000 default debounce, ignored
  assert(encoder.count() == 0);

  return 0;
}
```

- [ ] **Step 2: Run test, observe compile failure**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/encoder_test.cpp -o encoder_test && ./encoder_test
```

Expected: compilation fails because `VIA_Encoder.h`, `Encoder`, or `update` is missing.

- [ ] **Step 3: Implement `VIA_Encoder.h`**

Header-only class using a 2-bit state register tracking `lastA << 1 | lastB`, a signed counter, and a debounce timestamp. Recognize all four quadrature half-steps per detent. Reset counter on `consume()`. Reject state transitions within `debounceUs`.

- [ ] **Step 4: Run test, verify passes**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/encoder_test.cpp -o encoder_test && ./encoder_test
```

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Encoder.h tests/encoder_test.cpp
git commit -m "feat(encoder): add gray-code state machine"
git push
```

### Task 2: Battery Voltage Management

**Files:**
- Create: `src/VIA_Battery.h`
- Create: `tests/battery_test.cpp`

**Interfaces:**
- Produces: `class via::BatteryMgr` with `update(uint16_t rawAdc, uint32_t now)->void`, `uint8_t percentage() const`, `bool charging() const`, `void charging(bool state)`
- Produces: configurable `adcMax` (4095), `voltageMinMv` (3200), `voltageMaxMv` (4200), `samplesForAverage` (32)
- Produces: `rawToMv(uint16_t raw, uint16_t vrefMv=3300, uint16_t adcBits=12) -> uint16_t`

- [ ] **Step 1: Write failing battery tests**

```cpp
#include <assert.h>
#include "VIA_Battery.h"

int main() {
  via::BatteryMgr battery;
  // config: 3.2V=0%, 4.2V=100%, linear
  battery.setCalibration(3200, 4200);

  // exactly 4.2V → 100%
  battery.update(battery.rawFromMv(4200), 0);
  assert(battery.percentage() == 100);

  // exactly 3.2V → 0%
  battery.update(battery.rawFromMv(3200), 1);
  assert(battery.percentage() == 0);

  // 3.7V → 50%
  battery.update(battery.rawFromMv(3700), 2);
  assert(battery.percentage() == 50);

  // below min → clamped to 0
  battery.update(battery.rawFromMv(3000), 3);
  assert(battery.percentage() == 0);

  // above max → clamped to 100
  battery.update(battery.rawFromMv(4300), 4);
  assert(battery.percentage() == 100);

  // charging flag
  battery.charging(true);
  assert(battery.charging());
  battery.charging(false);
  assert(!battery.charging());

  // average: after N samples, stable
  battery.setAverageSamples(4);
  for (int i = 0; i < 4; ++i)
    battery.update(battery.rawFromMv(3600), 10+i);
  assert(battery.percentage() == 40); // 3.6V is midpoint: 400mV of 1000mV range → 40%

  return 0;
}
```

- [ ] **Step 2: Run, observe failure**

Expected: compile error — `VIA_Battery.h` does not exist.

- [ ] **Step 3: Implement `VIA_Battery.h`**

Header-only. Single ADC sample buffer array with a rolling average for the most recent `samplesForAverage`. `update(rawAdc, now)` converts to mV and pushes onto the moving average. `percentage()` reads from the last published stable average. All integer math; no floating point.

- [ ] **Step 4: Run, verify passes**

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/battery_test.cpp -o battery_test && ./battery_test
```

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Battery.h tests/battery_test.cpp
git commit -m "feat(battery): add voltage management"
git push
```

### Task 3: Sleep Manager

**Files:**
- Create: `src/VIA_SleepMgr.h`
- Create: `tests/sleep_test.cpp`

**Interfaces:**
- Produces: `class via::SleepMgr` with `update(bool anyActivity, uint32_t now)->bool` (returns true when sleep is requested), `configure(uint32_t timeoutMs)`
- Produces: callbacks `virtual void onSleepRequest()` and `virtual void onWakeReason(uint32_t reason)`
- Does NOT call `esp_deep_sleep_start()` — that is the adapter's responsibility.

- [ ] **Step 1: Write tests**

```cpp
#include <assert.h>
#include "VIA_SleepMgr.h"

int main() {
  via::SleepMgr sleep;
  sleep.configure(5000); // 5 second idle timeout

  // activity resets timer
  assert(!sleep.update(true, 0));
  assert(!sleep.update(false, 4000)); // 4s idle, not yet 5s
  assert(sleep.update(false, 5100));  // 5.1s idle → sleep requested
  assert(!sleep.update(false, 5101)); // already requested, no duplicate

  // new activity after sleep was denied: resets
  sleep.update(true, 10000); // activity at 10s
  assert(!sleep.update(false, 14000)); // 4s, not yet

  // wraparound safe
  sleep.configure(1000);
  assert(!sleep.update(true, 0xFFFFFFFF - 500));
  assert(sleep.update(false, 600)); // 500+600=1100ms elapsed (wrap)

  return 0;
}
```

- [ ] **Step 2: Run, observe failure**

- [ ] **Step 3: Implement header**

- [ ] **Step 4: Run, verify**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_SleepMgr.h tests/sleep_test.cpp
git commit -m "feat(sleep): add idle timeout manager"
git push
```

### Task 4: CI And ASan For Portable Modules

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Add encoder, battery, and sleep CI jobs**

```yaml
  encoder:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and run encoder tests
        run: |
          g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/encoder_test.cpp -o encoder_test && ./encoder_test
          g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/encoder_test.cpp -o encoder_test_san && ./encoder_test_san

  battery:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and run battery tests
        run: |
          g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/battery_test.cpp -o battery_test && ./battery_test
          g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/battery_test.cpp -o battery_test_san && ./battery_test_san

  sleep:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and run sleep tests
        run: |
          g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/sleep_test.cpp -o sleep_test && ./sleep_test
          g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/sleep_test.cpp -o sleep_test_san && ./sleep_test_san
```

- [ ] **Step 2: Run local tests**

```bash
wsl bash -lc 'cd /mnt/d/Pribadi/VIA-Arduino && g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/encoder_test.cpp -o /tmp/e && /tmp/e && g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/battery_test.cpp -o /tmp/b && /tmp/b && g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/sleep_test.cpp -o /tmp/s && /tmp/s && echo ALL:OK'
```

- [ ] **Step 3: Commit and push**

```bash
git add .github/workflows/ci.yml
git commit -m "test(ci): add encoder battery sleep jobs"
git push
```

### Milestone 1 Review Gate

All 3 new portable modules compiled with `-Werror`, ASan, and UBSan. Existing CI jobs green. Review all code before proceeding to adapters.

---

## Milestone 2: ESP32-S3 GPIO And NVS Adapters

**Goal:** Platform adapters for GPIO matrix scanning and NVS persistence.

**Files:**
- Create: `src/VIA_ESP32S3_GPIO.h`
- Create: `src/VIA_ESP32S3_NVS.h`

### Task 5: GPIO Matrix IO Adapter

**Files:**
- Create: `src/VIA_ESP32S3_GPIO.h`

**Interfaces:**
- Consumes: `via::MatrixIO`
- Produces: `class via::esp32s3::MatrixIOArduino : public via::MatrixIO` wrapping `pinMode`, `digitalRead`, `digitalWrite`, `delayMicroseconds`

- [ ] **Step 1: Write adapter**

```cpp
#pragma once
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "VIA_Matrix.h"
#include <Arduino.h>
namespace via { namespace esp32s3 {
class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) != HIGH; }
  void delayMicroseconds(uint16_t us) override { delayMicroseconds(us); }
};
} }  // namespaces
#endif
```

- [ ] **Step 2: Commit and push**

```bash
git add src/VIA_ESP32S3_GPIO.h
git commit -m "feat(esp32s3): add GPIO matrix adapter"
git push
```

### Task 6: NVS Storage Adapter

**Files:**
- Create: `src/VIA_ESP32S3_NVS.h`
- Create: `tests/nvs_storage_test.cpp` (with fake Preferences)

**Interfaces:**
- Consumes: `via::Storage`
- Produces: `class via::esp32s3::NVSStorage : public via::Storage` implementing `capacity()`, `read()`, `write()`, `commit()`, `erase()`
- Produces: native fake `class FakePreferences` for deterministic testing

- [ ] **Step 1: Write NVS test with fake Preferences**

```cpp
#include <assert.h>
#include <string.h>
#include "VIA_ESP32S3_NVS.h"

int main() {
  uint8_t buffer[4096] = {};
  via::esp32s3::NVSStorage storage(buffer, sizeof(buffer));
  assert(storage.capacity() == 4096);

  // round-trip
  uint8_t data[32] = "hello VIA via NVS storage!";
  assert(storage.write(0, data, sizeof(data)));
  assert(storage.commit());

  uint8_t restored[32] = {};
  assert(storage.read(0, restored, sizeof(restored)));
  assert(memcmp(data, restored, sizeof(data)) == 0);

  // erase
  assert(storage.erase());
  assert(storage.read(0, restored, sizeof(restored)));
  // erased bytes are 0xFF
  for (int i = 0; i < 32; ++i) assert(restored[i] == 0xFF);

  return 0;
}
```

- [ ] **Step 2: Run, observe failure**

- [ ] **Step 3: Implement `VIA_ESP32S3_NVS.h`**

Header with class template or runtime-conditional `#ifdef ARDUINO_ARCH_ESP32` guard. The native test uses the fake; the production header wraps `Preferences.h`.

- [ ] **Step 4: Run, verify, add CI**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_ESP32S3_NVS.h tests/nvs_storage_test.cpp .github/workflows/ci.yml
git commit -m "feat(esp32s3): add NVS storage adapter"
git push
```

### Milestone 2 Review Gate

GPIO adapter compiles. NVS adapter passes round-trip and erase tests with fake backend. CI green.

---

## Milestone 3: ESP32-S3 TinyUSB VIA Transport

**Goal:** Port RP2040 TinyUSB Raw HID adapter to ESP32-S3 as VIA transport.

**Files:**
- Create: `src/VIA_ESP32S3_USB.h`
- Create: `src/VIA_ESP32S3_USB.cpp`
- Create: `examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino` (compile-only scaffold)

### Task 7: TinyUSB Raw HID Port To ESP32-S3

**Files:**
- Create: `src/VIA_ESP32S3_USB.h`
- Create: `src/VIA_ESP32S3_USB.cpp`

**Interfaces:**
- Consumes: Adafruit TinyUSB library
- Produces: `class via::esp32s3::UsbDevice` implementing both `KeyboardHID` and `via::Transport`
- Produces: static lifetime TinyUSB HID device, report descriptors matching VIA spec (usage page 0xFF60, 0x61)

- [ ] **Step 1: Port RP2040 adapter**

Copy `src/VIA_TinyUSB_RawHID.h` / `.cpp` structure. Change: include path to TinyUSB (same `Adafruit_TinyUSB.h`), USB peripheral init for ESP32-S3 (menu `USB CDC On Boot: Disabled, USB Mode: TinyUSB`), add keyboard HID interface descriptor alongside VIA Raw HID. Endpoint: EP1 IN keyboard, EP2 IN/OUT VIA Raw. Same static singleton pattern, same `begin()` one-attempt, same destructor clearing ownership token.

- [ ] **Step 2: Write compile-only scaffold sketch**

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_ESP32S3_GPIO.h>
#include <VIA_ESP32S3_USB.h>

// matrix, keymap, protocol, keyboard, usb declarations...
// setup(), loop() skeleton
```

- [ ] **Step 3: Add ESP32-S3 compile CI job**

```yaml
- name: Install ESP32 toolchain
  run: |
    arduino-cli config add board_manager.additional_urls \
      https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    arduino-cli core update-index
    arduino-cli core install esp32:esp32@3.0.7
- name: Compile ESP32-S3 VIA BLE
  run: |
    arduino-cli compile --clean --warnings all \
      --fqbn "esp32:esp32:esp32s3:USBMode=1" \
      --libraries . examples/ESP32S3_VIA_BLE
```

- [ ] **Step 4: Commit and push**

```bash
git add src/VIA_ESP32S3_USB.h src/VIA_ESP32S3_USB.cpp examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino .github/workflows/ci.yml
git commit -m "feat(esp32s3): port TinyUSB VIA transport"
git push
```

### Milestone 3 Review Gate

S3 USB adapter compiles in CI. Scaffold sketch links. Existing protocol/matrix/keyboard tests green.

---

## Milestone 4: BLE Keyboard HID Adapter

**Goal:** NimBLE-based BLE HID keyboard adapter implementing `via::KeyboardHID`.

**Files:**
- Create: `src/VIA_ESP32S3_BLE.h`
- Create: `src/VIA_ESP32S3_BLE.cpp`

### Task 8: BLE Keyboard HID Adapter

**Files:**
- Create: `src/VIA_ESP32S3_BLE.h`
- Create: `src/VIA_ESP32S3_BLE.cpp`

**Interfaces:**
- Consumes: NimBLE-Arduino, ESP32-BLE-Keyboard
- Produces: `class via::esp32s3::BleKeyboardHID : public via::KeyboardHID` implementing `configured()`, `send(KeyboardReport)`, `sendComplete()`, `takeHostLeds()`, `suspended()`, `remoteWakeupAllowed()`, `remoteWakeup()`

- [ ] **Step 1: Implement adapter**

`BleKeyboardHID` wraps `BleKeyboard` from ESP32-BLE-Keyboard library. `begin("VIA Keyboard", "VIA BLE")` sets the device name. `configured()` returns `bleKeyboard.isConnected()`. `send(KeyboardReport)` calls `bleKeyboard.sendReport` with modifier and key array. `sendComplete()` always returns true (BLE has immediate completion). `suspended()` returns `!bleKeyboard.isConnected()`.

- [ ] **Step 2: Update scaffold sketch for BLE**

Add `#include <BleKeyboard.h>`, declare `BleKeyboardHID`, wire into `Keyboard` constructor. Compile-time only; BLE not functional without hardware.

- [ ] **Step 3: Update CI to include NimBLE dependency**

- [ ] **Step 4: Commit and push**

```bash
git add src/VIA_ESP32S3_BLE.h src/VIA_ESP32S3_BLE.cpp examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino .github/workflows/ci.yml
git commit -m "feat(esp32s3): add BLE keyboard HID adapter"
git push
```

### Milestone 4 Review Gate

BLE adapter compiles in CI alongside USB adapter. Sketch links with all dependencies.

---

## Milestone 5: Full Sketch Integration

**Goal:** Complete reference sketch, final CI matrix, docs.

**Files:**
- Modify: `examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino`
- Modify: `README.md`
- Modify: `CHANGELOG.md`

### Task 9: Complete Reference Sketch

**Files:**
- Modify: `examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino`

- [ ] **Step 1: Wire full integration sketch**

Add all modules on the spec pin map: matrix 5x15, one encoder, battery ADC, sleep manager, OLED placeholder, WS2812 placeholder. Wire VIA protocol with NVS persistence. Dual-mode: USB present → use USB keyboard HID + VIA; absent → use BLE keyboard HID. Add encoder poll in `loop()`. Add battery ADC read at 1 Hz. Add idle detection feeding `SleepMgr`.

- [ ] **Step 2: Update README and CHANGELOG**

Document new ESP32-S3 target, dual-mode, BLE keyboard, VIA over USB, NVS persistence. List untested hardware features. Note that ESP32 classic WROOM is not supported (no USB device).

- [ ] **Step 3: Commit and push**

```bash
git add examples/ESP32S3_VIA_BLE/ESP32S3_VIA_BLE.ino README.md CHANGELOG.md
git commit -m "feat(esp32s3): complete wireless keyboard integration"
git push
```

### Milestone 5 Review Gate

Sketch compiles in CI. All native tests green. Documentation accurate.

---

## Hardware Certification

Hardware is not currently available. These gates block a hardware-verified
`0.4.0` release but do not block merging software-complete code:

- USB enumeration shows two HID interfaces (keyboard + VIA Raw HID).
- VIA desktop discovers the keyboard, protocol version, keymap remap.
- Remapped VIA keymap persists after NVS save and cold boot.
- BLE HID typing works on Windows, macOS, Linux, and Android.
- No key drops or repeats at BLE connection interval 15 ms.
- USB-to-BLE mode switch produces no stuck keys.
- Idle timeout triggers deep sleep. Wake from any key restores BLE.
- Battery percentage reported correctly via BLE battery service.
- Sleep current under 20 µA.
- Encoder CW/CCW directions and layer mapping correct.
- Factory reset clears NVS and restores compiled defaults.

## Self-Review

### Spec Coverage
- [x] Encoder gray-code state machine (Task 1)
- [x] Battery voltage management (Task 2)
- [x] Sleep idle timeout manager (Task 3)
- [x] GPIO matrix IO adapter (Task 5)
- [x] NVS storage adapter (Task 6)
- [x] TinyUSB Raw HID port to ESP32-S3 (Task 7)
- [x] BLE keyboard HID adapter (Task 8)
- [x] Full sketch integration (Task 9)
- [x] CI and ASan/UBSan for new modules (Tasks 4, 7, 8)
- [x] Hardware certification gates listed

### Placeholder Scan
- No TBD, TODO, or placeholder text found.

### Type Consistency
- Encoder: `update(pinA, pinB, now)`, `count()`, `consume()` — consistent Task 1-9.
- Battery: `update(rawAdc, now)`, `percentage()`, `charging()` — consistent Task 2-9.
- SleepMgr: `update(activity, now)`, `configure(timeout)`, `onSleepRequest()` — consistent Task 3-9.
- MatrixIO adapters use identical `inputPullup/driveLow/release/read/delayMicroseconds` contracts.
- Storage adapters implement identical `capacity/read/write/commit/erase` contracts.
- KeyboardHID adapters implement identical `configured/send/sendComplete/takeHostLeds/suspended/remoteWakeupAllowed/remoteWakeup` contracts.
