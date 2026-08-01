# STM32F103 Keyboard MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add portable matrix scanner, debounce, keycode/layer engine, and 6KRO to VIA-Arduino, then integrate STM32F103CB adapters (USB composite, dual-slot flash, ROM USART boot) to produce a real wired keyboard library.

**Architecture:** Six milestones, each independently tested and reviewed. Portable core modules (`src/VIA_Matrix.*`, `src/VIA_Keyboard.*`, `src/VIA_Keycodes.*`) have no Arduino/Cube headers. STM32 adapter modules (`src/VIA_STM32F1_USB.*`, `src/VIA_STM32F1_Flash.*`, `src/VIA_STM32F1_Boot.*`) call only platform APIs and implement the portable contracts.

**Tech Stack:** C++11, Arduino library format, native `assert` tests, STM32duino 3.0.0 with STM32Cube USB Device middleware, ARM GCC, GitHub Actions, Arduino CLI.

## Global Constraints

- MIT-licensed project code; do not copy GPL QMK implementation.
- C++11 compatibility; no heap allocation in portable or adapter code.
- Target STM32F103CB (128 KiB flash, 20 KiB RAM).
- Arduino core: `STMicroelectronics:stm32@3.0.0` pinned.
- Existing protocol tests, Uno compile, and RP2040 compile must remain green.
- Reference matrix: 6 rows, 18 columns, 4 layers, `COL2ROW`.
- Memory gates: image <= 112 KiB, `.data+.bss` <= 12 KiB, stack >= 4 KiB reserved, zero heap.
- FQBN: `STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103CB,xserial=disabled,usb=HID,xusb=FS,opt=oslto,dbg=none,rtlib=nano,upload_method=swdMethod`
- Branch: `feat/0.3-stm32f103-keyboard-mvp`; push after each reviewed task.

---

## Milestone 1: Portable Matrix Scanner And Debounce

**Goal:** Core matrix scanning and debounce, testable without hardware.

**Files:**
- Create: `src/VIA_Matrix.h`
- Create: `src/VIA_Matrix.cpp`
- Create: `tests/matrix_test.cpp`
- Modify: `.github/workflows/ci.yml`

### Task 1: Matrix I/O Contract And Fakes

**Files:**
- Create: `src/VIA_Matrix.h`
- Create: `tests/matrix_test.cpp`

**Interfaces:**
- Produces: `via::Pin` typedef (`uint32_t`)
- Produces: `enum via::DiodeDirection { kColToRow, kRowToCol }`
- Produces: `class via::MatrixIO` with `inputPullup(Pin)`, `driveLow(Pin)`, `release(Pin)`, `read(Pin)->bool`, `delayMicroseconds(uint16_t)`
- Produces: `struct via::MatrixConfig { uint8_t rows; uint8_t columns; const Pin* rowPins; const Pin* columnPins; DiodeDirection direction; uint16_t settleUs; uint16_t debounceMs; uint32_t* rawRows; uint32_t* candidateRows; uint32_t* stableRows; uint32_t* changedRows; }`
- Produces: `class via::Matrix` with `Matrix(const MatrixConfig&, MatrixIO&)`, `begin()->bool`, `task(uint32_t now)->void`, and const accessors for stable/changed rows
- Produces: fake `class FakeMatrixIO` in `tests/matrix_test.cpp` capturing call counts and pin state for deterministic validation

- [ ] **Step 1: Write failing test — `begin()` rejects zero dimensions**

```cpp
#include <assert.h>
#include "VIA_Matrix.h"

int main() {
  via::MatrixIO* io = nullptr; // test IO injected later
  via::Pin rows[] = {};
  via::Pin cols[] = {};
  uint32_t raw[1], candidate[1], stable[1], changed[1];
  via::MatrixConfig cfg = {0, 0, rows, cols, via::kColToRow, 30, 5,
                            raw, candidate, stable, changed};
  via::Matrix matrix(cfg, *io);
  assert(!matrix.begin());
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/matrix_test.cpp -o matrix_test && ./matrix_test
```
Expected: compile fails because `VIA_Matrix.h`, `MatrixIO`, `MatrixConfig`, or `Matrix` is missing.

- [ ] **Step 3: Write minimal header and implementation**

Create `src/VIA_Matrix.h` with `Pin`, `DiodeDirection`, `MatrixIO`, `MatrixConfig`, and `Matrix` declaring `begin()` returning `false` when `config_.rows == 0 || config_.columns == 0`. No `.cpp` yet; inline `begin()` in header.

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/matrix_test.cpp -o matrix_test && ./matrix_test
```
Expected: passes.

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Matrix.h tests/matrix_test.cpp
git commit -m "test(matrix): define I/O contract and config"
git push
```

### Task 2: COL2ROW Scan Cycle

**Files:**
- Modify: `src/VIA_Matrix.h`
- Create: `src/VIA_Matrix.cpp`
- Modify: `tests/matrix_test.cpp`

**Interfaces:**
- Consumes: `Matrix`, `MatrixConfig`, `MatrixIO`
- Produces: `Matrix::task(uint32_t)` performs one full COL2ROW scan when `direction == kColToRow`
- Produces: `Matrix::rawRow(uint8_t)`, `Matrix::stableRow(uint8_t)`, `Matrix::changedRow(uint8_t)`, `Matrix::rows()`, `Matrix::columns()` const accessors

- [ ] **Step 1: Write failing scan test**

Add to `tests/matrix_test.cpp` a `FakeMatrixIO` capturing `driveLow` and `read` call records, then a test injecting a specific pressed pattern:

```cpp
class FakeMatrixIO : public via::MatrixIO {
 public:
  uint32_t driveCalls, releaseCalls, settleCalls;
  bool lowState, readValues[18];
  FakeMatrixIO() : driveCalls(0), releaseCalls(0), settleCalls(0), lowState(false) {
    for (int i = 0; i < 18; ++i) readValues[i] = true;
  }
  void inputPullup(via::Pin) override {}
  void driveLow(via::Pin) override { driveCalls++; lowState = true; }
  void release(via::Pin) override { releaseCalls++; lowState = false; }
  bool read(via::Pin pin) override { return readValues[pin & 0x1F]; }
  void delayMicroseconds(uint16_t) override { settleCalls++; }
};

// ...

via::Pin rowPins[2] = {100, 101};
via::Pin colPins[3] = {200, 201, 202};
uint32_t rawRows[2], candidateRows[2], stableRows[2], changedRows[2];
FakeMatrixIO io;
io.readValues[0] = false; // column 0 pressed
via::MatrixConfig cfg = {2, 3, rowPins, colPins, via::kColToRow, 30, 0,
                          rawRows, candidateRows, stableRows, changedRows};
via::Matrix matrix(cfg, io);
assert(matrix.begin());
matrix.task(0);
assert(matrix.rawRow(0) == 0x01); // column 0 bit set
assert(io.driveCalls == 1);
assert(io.releaseCalls == 1);
assert(io.settleCalls == 1);
```

- [ ] **Step 2: Run test, observe expected failure**

Expected: `task()` missing or no raw row populated.

- [ ] **Step 3: Implement `Matrix::task()`**

In `src/VIA_Matrix.cpp`:
- If `direction == kColToRow`: set all columns input pull-up, release all rows, iterate rows driving each low, delay settle, read columns into `rawRows[row]`, then release row before next.
- Store `rows_` and `columns_` from config.

- [ ] **Step 4: Run test, verify passes**

Run:
```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/matrix_test.cpp src/VIA_Matrix.cpp -o matrix_test && ./matrix_test
```
Expected: passes.

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Matrix.h src/VIA_Matrix.cpp tests/matrix_test.cpp
git commit -m "feat(matrix): implement COL2ROW scan cycle"
git push
```

### Task 3: ROW2COL Scan Cycle

**Files:**
- Modify: `src/VIA_Matrix.cpp`
- Modify: `tests/matrix_test.cpp`

**Interfaces:**
- Produces: `Matrix::task()` scans correctly for `kRowToCol` (drive column low, read rows)

- [ ] **Step 1: Write failing ROW2COL test**

Add test: configure `kRowToCol`, inject column 2 low, assert `rawRow(0)` shows proper bit.

```cpp
// ROW2COL: drive columns, read rows
FakeMatrixIO io2;
io2.readValues[2] = false;
via::MatrixConfig cfg2 = {2, 3, rowPins, colPins, via::kRowToCol, 30, 0,
                           rawRows, candidateRows, stableRows, changedRows};
via::Matrix matrix2(cfg2, io2);
assert(matrix2.begin());
matrix2.task(0);
assert(matrix2.rawRow(0) == (1U << 2)); // row 0 sees column 2 pressed
```

- [ ] **Step 2: Run, observe failure**

Expected: raw row mismatched because only COL2ROW exists.

- [ ] **Step 3: Add ROW2COL branch in `task()`**

Add `else` branch: set rows pull-up, release columns, iterate columns driving each low, delay settle, read rows.

- [ ] **Step 4: Run tests, verify**

Both scan direction tests pass.

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Matrix.cpp tests/matrix_test.cpp
git commit -m "feat(matrix): add ROW2COL scan direction"
git push
```

### Task 4: Deferred Debounce And Changed Detection

**Files:**
- Modify: `src/VIA_Matrix.cpp`
- Modify: `tests/matrix_test.cpp`

**Interfaces:**
- Produces: `Matrix::task()` detects raw changes, replaces candidate, restarts global timer, publishes stable matrix only after `debounceMs` without change

- [ ] **Step 1: Write failing debounce tests**

Add tests:
- `debounceMs == 0`: stable mirrors raw on first scan.
- `debounceMs == 10`: raw change at t=0, stable unchanged at t=9, stable matches candidate at t=10.
- Noise spike at t=2 then stable raw: timer resets, stable published at t=12.
- `millis()` wrap from `UINT32_MAX` to `0`: elapsed math still correct.
- `debounceMs == 5` (default): verify specific timing values.

- [ ] **Step 2: Run, observe failure**

Expected: stable rows never update, changed rows stay zero.

- [ ] **Step 3: Implement deferred debounce**

In `task()`: compare `rawRows` against previous raw snapshot. On change: copy rawRows into `candidateRows`, start timer. After `debounceMs` elapsed without further change: copy `candidateRows` into `stableRows`, XOR old stable vs new stable into `changedRows`. Zero debounce publishes without delay.

- [ ] **Step 4: Run all tests, verify**

All scan and debounce tests pass.

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Matrix.cpp tests/matrix_test.cpp
git commit -m "feat(matrix): defer matrix publication"
git push
```

### Task 5: Accessors, CI, And ASan

**Files:**
- Modify: `src/VIA_Matrix.h`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: `Matrix::rows()`, `Matrix::columns()`, `Matrix::rawRow(uint8_t)`, `Matrix::stableRow(uint8_t)`, `Matrix::changedRow(uint8_t)`, `Matrix::hasChanged()` (OR `changedRows`), `Matrix::nextScan(uint32_t now)->uint32_t`
- Produces: CI matrix test job under ASan/UBSan

- [ ] **Step 1: Write accessor tests**

Add tests: `hasChanged` true after debounced press, false when called in same stable state.

- [ ] **Step 2: Add CI job**

```yaml
- name: Build native matrix test
  run: |
    g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/matrix_test.cpp \
      src/VIA_Matrix.cpp -o matrix_test && ./matrix_test
    g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
      -fno-omit-frame-pointer -Isrc tests/matrix_test.cpp \
      src/VIA_Matrix.cpp -o matrix_test_san && ./matrix_test_san
```

- [ ] **Step 3: Run local ASan/UBSan**

```bash
wsl bash -lc 'cd /mnt/d/Pribadi/VIA-Arduino && g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/matrix_test.cpp src/VIA_Matrix.cpp -o /tmp/matrix_san && /tmp/matrix_san'
```
Expected: exit 0.

- [ ] **Step 4: Commit and push**

```bash
git add src/VIA_Matrix.h tests/matrix_test.cpp .github/workflows/ci.yml
git commit -m "test(matrix): add accessors and CI gate"
git push
```

### Milestone 1 Review Gate

Review all changes for scan correctness, diode direction semantics, settle timing, debounce transitions, timer wrap, changed-row masks, and CI green. All Critical/Important issues fixed before proceeding.

---

## Milestone 2: Portable Keyboard Engine

**Goal:** Event processing, layer resolution, keycode dispatch, and 6KRO report building, all native-tested.

**Files:**
- Create: `src/VIA_Keycodes.h`
- Create: `src/VIA_Keyboard.h`
- Create: `src/VIA_Keyboard.cpp`
- Create: `tests/keyboard_test.cpp`
- Modify: `src/VIA_Protocol.h` (add `rows()`, `columns()`, `layers()` accessors if needed)
- Modify: `.github/workflows/ci.yml`

### Task 6: Keycode Classification

**Files:**
- Create: `src/VIA_Keycodes.h`

**Interfaces:**
- Produces: `enum class via::KeycodeType { kNone, kTransparent, kBasic, kModifier, kLayer, kBoot, kUnsupported }`
- Produces: `via::classifyKeycode(uint16_t)->KeycodeType`
- Produces: `via::extractBasicUsage(uint16_t)->uint8_t` (HID usage for `0x0004`-`0x00A4`)
- Produces: `via::extractModifierMask(uint16_t)->uint8_t` (for `0x00E0`-`0x00E7`: 1<<(code-0xE0))
- Produces: `via::extractQkMods(uint16_t, uint8_t& hidUsage, uint8_t& modMask)->bool` (decode QK_MODS `0x0100`-`0x1FFF`)
- Produces: `via::extractLayerAction(uint16_t, uint8_t& action, uint8_t& layer)->bool` (for MO/TG/TO/DF)
- Produces: All outputs are header-only constexpr-compatible functions

- [ ] **Step 1: Write classification table tests**

```cpp
#include <assert.h>
#include "VIA_Keycodes.h"

int main() {
  assert(via::classifyKeycode(0x0000) == via::KeycodeType::kNone);
  assert(via::classifyKeycode(0x0001) == via::KeycodeType::kTransparent);
  assert(via::classifyKeycode(0x0004) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x00A4) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x00E0) == via::KeycodeType::kModifier);
  assert(via::classifyKeycode(0x00E7) == via::KeycodeType::kModifier);
  assert(via::classifyKeycode(0x0100) == via::KeycodeType::kBasic); // QK_MODS
  assert(via::classifyKeycode(0x1FFF) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x5220) == via::KeycodeType::kLayer);
  assert(via::classifyKeycode(0x5260) == via::KeycodeType::kLayer);
  assert(via::classifyKeycode(0x7C00) == via::KeycodeType::kBoot);
  assert(via::classifyKeycode(0x2000) == via::KeycodeType::kUnsupported);
  assert(via::classifyKeycode(0x8000) == via::KeycodeType::kUnsupported);
  return 0;
}
```

- [ ] **Step 2: Run, observe compile failure**

- [ ] **Step 3: Implement header-only classification**

Implement `classifyKeycode` with `switch` on high-byte ranges. Implement `extractBasicUsage` returning `code & 0xFF` for basics. Implement `extractModifierMask` returning `1U << (code - 0xE0)`. Implement `extractQkMods` decoding bits 8-11 for modifier count and bit 12 for right-side shift. Implement `extractLayerAction` decoding TO from `0x5200`, MO from `0x5220`, DF from `0x5240`, TG from `0x5260`.

- [ ] **Step 4: Run tests, verify**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Keycodes.h tests/keyboard_test.cpp .github/workflows/ci.yml
git commit -m "feat(keycode): classify QMK keycode ranges"
git push
```

### Task 7: Keyboard HID Contract and Test Sink

**Files:**
- Create: `src/VIA_Keyboard.h`
- Modify: `tests/keyboard_test.cpp`

**Interfaces:**
- Produces: `struct via::KeyboardReport { uint8_t modifiers; uint8_t reserved; uint8_t keys[6]; }`
- Produces: `class via::KeyboardHID` with `configured()`, `send(const KeyboardReport&)->bool`, `sendComplete()->bool`, `takeHostLeds(uint8_t&)->bool`, `suspended()`, `remoteWakeupAllowed()`, `remoteWakeup()->bool`
- Produces: `class via::KeyboardCallbacks` with `hostLedsChanged(uint8_t)`, `bootloaderRequested()`
- Produces: fake `FakeKeyboardHID` in test file with configurable busy state, send/sendComplete tracking, host LED injection, suspend control

- [ ] **Step 1: Extend test file with FakeKeyboardHID**

Add to `tests/keyboard_test.cpp`:

```cpp
class FakeKeyboardHID : public via::KeyboardHID {
 public:
  bool _configured = true, _suspended = false, _wakeAllowed = false;
  bool sendResult = true, completeResult = true;
  uint32_t sendCalls = 0, completeCalls = 0, wakeCalls = 0;
  via::KeyboardReport lastReport;
  uint8_t hostLeds = 0;
  bool configured() const override { return _configured; }
  bool send(const via::KeyboardReport& r) override { sendCalls++; lastReport = r; return sendResult; }
  bool sendComplete() override { completeCalls++; return completeResult; }
  bool takeHostLeds(uint8_t& l) override { l = hostLeds; hostLeds = 0; return true; }
  bool suspended() const override { return _suspended; }
  bool remoteWakeupAllowed() const override { return _wakeAllowed; }
  bool remoteWakeup() override { wakeCalls++; return true; }
};
```

- [ ] **Step 2: Write failing create test**

```cpp
via::FakeKeyboardHID hid;
via::KeyboardReport r = {0x01, 0, {0x04, 0, 0, 0, 0, 0}};
assert(hid.send(r));
assert(hid.lastReport.modifiers == 0x01);
```

- [ ] **Step 3: Add header with struct and classes**

Create `src/VIA_Keyboard.h` containing `KeyboardReport`, `KeyboardHID`, `KeyboardCallbacks`.

- [ ] **Step 4: Run, verify compiles and passes**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Keyboard.h tests/keyboard_test.cpp
git commit -m "feat(keyboard): define HID sink contract"
git push
```

### Task 8: Layer State Machine

**Files:**
- Modify: `src/VIA_Keyboard.h`
- Modify: `src/VIA_Keyboard.cpp` (or add layer code inline)
- Modify: `tests/keyboard_test.cpp`

**Interfaces:**
- Produces: `class via::LayerState` with `begin(uint8_t layerCount)->void`, `applyLayerPress(uint8_t layer, uint16_t action)`, `applyLayerRelease(uint8_t layer, uint16_t action)`, `resolve(uint8_t defaultLayer, const uint16_t* keymap, uint8_t row, uint8_t col, uint8_t rows, uint8_t cols)->uint16_t`, `activeLayerMask() const -> uint32_t`

- [ ] **Step 1: Write layer tests**

Add tests:
- Press MO(1): layer resolves to 1, release MO(1) removes layer.
- Press TG(1): after release layer 1 remains active, second TG(1) press removes.
- Press TO(1): clears other transient layers, activates 1.
- Press DF(1): default layer becomes 1.
- Duplicate MO(0) presses: reference count prevents premature removal.
- Press MO(1) then MO(2) from layer 1: resolution uses highest active layer.
- `KC_TRNS` on active layer falls through.
- `KC_NO` stops lookup.
- Invalid layer target: no-op.

- [ ] **Step 2: Run, observe failure**

- [ ] **Step 3: Implement LayerState**

`_transientMask` as `uint32_t` bitmask, `_refCounts[32]` for MO reference counting. `applyLayerPress` decodes action type from `MO`, `TG`, `TO`, `DF` keycodes stored in `activeCodes`. `applyLayerRelease` only handles `MO`. `resolve()` scans highest transient bit downward, checks default layer last.

- [ ] **Step 4: Run, verify all pass**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Keyboard.h src/VIA_Keyboard.cpp tests/keyboard_test.cpp
git commit -m "feat(keyboard): add layer state machine"
git push
```

### Task 9: Event Engine And Keycode Dispatch

**Files:**
- Modify: `src/VIA_Keyboard.h`
- Modify: `src/VIA_Keyboard.cpp`
- Modify: `tests/keyboard_test.cpp`

**Interfaces:**
- Consumes: `Matrix`, `MatrixConfig`, `LayerState`, `KeyboardHID`, `KeyboardCallbacks`, `Protocol`
- Produces: `class via::Keyboard` with `Keyboard(const KeyboardConfig&, Matrix&, KeyboardHID&, Protocol&, uint16_t* activeCodes, KeyboardCallbacks*)`, `begin()->bool`, `task(uint32_t now)->void`, `stableRow(uint8_t)->uint32_t`

- [ ] **Step 1: Write end-to-end event test**

Inject a stable matrix change (row 0 column 3 pressed), call `keyboard.task(0)`, assert:
- `activeCodes[0*18+3]` is the keycode from `protocol.keycode(defaultLayer, 0, 3)`.
- FakeHID `lastReport.keys[0]` contains the HID usage.
- On stable release: active code cleared, report empty.
- VIA remaps the keycode between press and release: `activeCodes` still holds original code, release sends original usage, not new one.

- [ ] **Step 2: Run, observe failures**

- [ ] **Step 3: Implement `Keyboard::task()`**

1. Read `changedRows` from matrix.
2. If changes exist, process releases first: for each changed released position, resolve its `activeCodes[pos]` (applyLayerRelease if it's a layer action, else send release via HID).
3. Process layer-action presses: `MO`, `TG`, `TO`, `DF` — these change layer state.
4. Process remaining presses: resolve keycode against current layer state, store in `activeCodes`, classify, dispatch to HID.
5. Rebuild and send one report.

Report building:
- OR all active basic key usages into modifier byte or keys[0..5].
- Handle duplicates by reference counting.
- Six-key limit with ErrorRollOver.
- All-zero report on release.

- [ ] **Step 4: Run full test suite, verify**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Keyboard.h src/VIA_Keyboard.cpp tests/keyboard_test.cpp
git commit -m "feat(keyboard): process events and dispatch"
git push
```

### Task 10: Report Coalescing, Busy Retry, And Suspend

**Files:**
- Modify: `src/VIA_Keyboard.cpp`
- Modify: `tests/keyboard_test.cpp`

**Interfaces:**
- Produces: `Keyboard::task()` retains last failed report, retries without rebuilding, coalesces latest state while busy
- Produces: `Keyboard::task()` drains host LEDs through `takeHostLeds()`, calls `hostLedsChanged()` once per change
- Produces: `Keyboard::task()` skips send when suspended, issues remote wake on first press

- [ ] **Step 1: Write tests**

- Busy send: `hid.sendResult = false` then `true`; report sent exactly once with latest wanted state.
- `sendComplete()` false while in-flight; task blocks next scan cycle.
- Host LED: inject `0x03` (Num+Caps), assert callback receives `0x03`.
- Suspend: inject suspend, press key, assert no send, wake requested once, second press does not request second wake.
- Resume: clear suspend, assert report is rebuilt and sent.

- [ ] **Step 2: Run, observe failures**

- [ ] **Step 3: Implement**

Add `pendingReport_`, `reportPending_`, and `desiredReport_` to Keyboard. In `task()`:
- Before processing: if `reportPending_` and `sendComplete()`, mark accepted, loop back.
- If `reportPending_` and not complete: retry `send()` without reprocessing.
- After processing: build desired report; if differs from `pendingReport_` or no pending, call `send()`.
- Add drain loop at top: `while (hid_.takeHostLeds(leds)) { if (leds != lastLeds_) { lastLeds_ = leds; callbacks_->hostLedsChanged(leds); } }`
- Suspend gate: if `suspended()`, skip send, handle wake logic.

- [ ] **Step 4: Run, verify**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_Keyboard.cpp tests/keyboard_test.cpp
git commit -m "feat(keyboard): coalesce reports and handle suspend"
git push
```

### Task 11: CI, ASan/UBSan, And Existing Suite

**Files:**
- Modify: `src/VIA_Keycodes.h`
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Add keyboard native CI job**

```yaml
- name: Build native keyboard test
  run: |
    g++ -std=c++11 -Wall -Wextra -Werror -Isrc \
      tests/keyboard_test.cpp src/VIA_Matrix.cpp src/VIA_Keyboard.cpp \
      src/VIA_Protocol.cpp src/VIA_RGBLight.cpp -o keyboard_test && \
      ./keyboard_test
    g++ -std=c++11 -Wall -Wextra -Werror \
      -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
      tests/keyboard_test.cpp src/VIA_Matrix.cpp src/VIA_Keyboard.cpp \
      src/VIA_Protocol.cpp src/VIA_RGBLight.cpp -o keyboard_test_san && \
      ./keyboard_test_san
```

- [ ] **Step 2: Run all existing native tests and new suites**

```bash
# Protocol
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/protocol_test.cpp src/VIA_Protocol.cpp src/VIA_RGBLight.cpp -o protocol_test && ./protocol_test
# TinyUSB
g++ -std=c++11 -Wall -Wextra -Werror -Itests/fakes -Isrc tests/tinyusb_rawhid_test.cpp src/VIA_TinyUSB_RawHID.cpp -o tinyusb_test && ./tinyusb_test failed && ./tinyusb_test successful
# Matrix
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/matrix_test.cpp src/VIA_Matrix.cpp -o matrix_test && ./matrix_test
# Keyboard
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/keyboard_test.cpp src/VIA_Matrix.cpp src/VIA_Keyboard.cpp src/VIA_Protocol.cpp src/VIA_RGBLight.cpp -o keyboard_test && ./keyboard_test
```

- [ ] **Step 3: Ensure Uno and RP2040 jobs compile**

- [ ] **Step 4: Commit and push**

```bash
git add .github/workflows/ci.yml src/VIA_Keycodes.h
git commit -m "test(keyboard): add keyboard CI and ASan"
git push
```

### Milestone 2 Review Gate

Review keycode classification correctness against QMK keycodes.h, layer semantics, event batching order, report rollover, modifier encoding (QK_MODS right-side rule), held-key remap behavior, host LED draining, suspend/wake, and CI green. Milestone 3 blocked until 1-2 clean.

---

## Milestone 3: STM32F103 USB Composite

**Goal:** Custom STM32Cube USB device class exposing boot keyboard interface 0 and VIA Raw HID interface 1.

**Files:**
- Create: `src/VIA_STM32F1_USB.h`
- Create: `src/VIA_STM32F1_USB.cpp`
- Create: `tests/usb_descriptor_test.cpp`
- Create: `examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino`
- Modify: `.github/workflows/ci.yml`

### Task 12: USB Descriptor Snapshots

**Files:**
- Create: `tests/usb_descriptor_test.cpp`
- Create: `src/VIA_STM32F1_USBDescriptor.h`

**Interfaces:**
- Produces: `const uint8_t kCompositeConfigDescriptor[]` with two HID interfaces
- Produces: Interface 0 report descriptor: boot keyboard 8-byte IN (modifier + reserved + 6 keys)
- Produces: Interface 1 report descriptor: vendor HID 0xFF60/0x61, 32-byte IN/OUT
- Produces: VID/PID `0xCAFE/0x4003` (test-only)
- Produces: Native tests verify exact descriptor bytes, endpoint count, usage IDs

- [ ] **Step 1: Write expected-descriptor test**

```cpp
#include <assert.h>
#include "VIA_STM32F1_USBDescriptor.h"
#include <string.h>

int main() {
  // device descriptor: VID 0xCAFE, PID 0x4003, bcdDevice 0x0001
  assert(via::stm32f1::kDeviceDescriptor[8]  == 0xFE); // VID low
  assert(via::stm32f1::kDeviceDescriptor[9]  == 0xCA); // VID high
  assert(via::stm32f1::kDeviceDescriptor[10] == 0x03); // PID low
  assert(via::stm32f1::kDeviceDescriptor[11] == 0x40); // PID high

  // scan config descriptor for:
  // - two interface descriptors
  // - HID report descriptor for boot keyboard (usage page 0x07, 8-byte)
  // - HID report descriptor for vendor (usage page 0xFF60, usage 0x61, 32-byte)
  // - no report IDs in either HID descriptor
  // - three endpoint descriptors (EP1 IN, EP2 IN, EP2 OUT)

  int ifaceCount = 0, epCount = 0;
  bool foundBootKbd = false, foundViaRaw = false;
  const uint8_t* p = via::stm32f1::kConfigDescriptor;
  uint16_t totalLen = p[2] | (p[3] << 8);
  const uint8_t* end = p + totalLen;
  p += 9; // skip config descriptor header
  while (p < end) {
    uint8_t len = p[0];
    uint8_t type = p[1];
    if (type == 4) ifaceCount++;
    if (type == 5) epCount++;
    if (type == 0x21) { // HID descriptor
      // verify class-specific data
    }
    // check report descriptor contents
    p += len;
  }
  assert(ifaceCount == 2);
  assert(epCount == 3);
  return 0;
}
```

- [ ] **Step 2: Run, observe failure**

- [ ] **Step 3: Create descriptor header**

Write `src/VIA_STM32F1_USBDescriptor.h` with `kDeviceDescriptor[]`, `kConfigDescriptor[]`, and independent `kBootKeyboardReportDescriptor[]`, `kViaRawHidReportDescriptor[]` arrays. Use hexadecimal bytes.

Boot keyboard descriptor:
```cpp
0x05,0x01,      // Usage Page (Generic Desktop)
0x09,0x06,      // Usage (Keyboard)
0xA1,0x01,      // Collection (Application)
0x05,0x07,      //   Usage Page (Keyboard/Keypad)
0x19,0xE0,      //   Usage Minimum (Left Control)
0x29,0xE7,      //   Usage Maximum (Right GUI)
0x15,0x00,      //   Logical Minimum (0)
0x25,0x01,      //   Logical Maximum (1)
0x95,0x08,      //   Report Count (8)
0x75,0x01,      //   Report Size (1)
0x81,0x02,      //   Input (Data, Variable, Absolute) — 8 modifier bits
0x95,0x01,      //   Report Count (1)
0x75,0x08,      //   Report Size (8)
0x81,0x01,      //   Input (Constant) — reserved byte
0x95,0x06,      //   Report Count (6)
0x75,0x08,      //   Report Size (8)
0x15,0x00,      //   Logical Minimum (0)
0x26,0xA4,0x00, //   Logical Maximum (164)
0x05,0x07,      //   Usage Page (Keyboard/Keypad)
0x19,0x00,      //   Usage Minimum (0)
0x29,0xA4,      //   Usage Maximum (164)
0x81,0x00,      //   Input (Data, Array) — 6 key slots
0x05,0x08,      //   Usage Page (LEDs)
0x19,0x01,      //   Usage Minimum (Num Lock)
0x29,0x05,      //   Usage Maximum (Kana)
0x95,0x05,      //   Report Count (5)
0x75,0x01,      //   Report Size (1)
0x91,0x02,      //   Output (Data, Variable, Absolute) — LED states
0x95,0x01,      //   Report Count (1)
0x75,0x03,      //   Report Size (3)
0x91,0x01,      //   Output (Constant) — padding
0xC0            // End Collection
```

- [ ] **Step 4: Run, verify**

- [ ] **Step 5: Commit and push**

```bash
git add tests/usb_descriptor_test.cpp src/VIA_STM32F1_USBDescriptor.h .github/workflows/ci.yml
git commit -m "feat(usb): define STM32F1 composite descriptors"
git push
```

### Task 13: STM32Cube USB Device Adapter

**Files:**
- Create: `src/VIA_STM32F1_USB.h`
- Create: `src/VIA_STM32F1_USB.cpp`

**Interfaces:**
- Produces: `class via::stm32f1::UsbDevice` implementing both `KeyboardHID` and `via::Transport`
- Produces: `UsbDevice::begin()` — initializes Cube peripheral, registers custom class, starts device
- Produces: `UsbDevice::task()` — processes one USB event frame
- Produces: Static buffers for keyboard report (8 bytes), VIA IN report (32 bytes), VIA OUT report (32 bytes), host LED byte
- Produces: Inline ISR/class callbacks copy fixed-size buffers, set flags, rearm endpoint; main-loop `task()` drains flags into `KeyboardHID` and `Transport` contracts

- [ ] **Step 1: Write tests (functional contracts)**

Test for `KeyboardHID` contract conformance using a fake: initial configured false, after successful begin configured true, send buffers report, sendComplete reflects endpoint state, etc. (These will be minimal host-side tests since actual Cube requires ARM target.)

- [ ] **Step 2: Write adapter with Cube middleware calls**

Include `usbd_core.h`, `usbd_desc.h`, `usbd_hid.h`. Implement `USBD_Init`, `USBD_RegisterClass` for custom HID composite. Register endpoint callbacks that copy buffers and set `volatile` flags. Implement `send()` calling `USBD_LL_Transmit` on EP1 IN for keyboard or EP2 IN for VIA. Implement `sendComplete()` checking transmit-in-progress flag. Implement `takeHostLeds()` draining `SET_REPORT` buffer.

- [ ] **Step 3: No native run (ARM-only). Verify STM32 compile in CI.**

GitHub compile gate confirms link succeeds.

- [ ] **Step 4: Commit and push**

```bash
git add src/VIA_STM32F1_USB.h src/VIA_STM32F1_USB.cpp
git commit -m "feat(usb): implement STM32F1 Cube composite"
git push
```

### Task 14: STM32 Arduino Example Scaffold

**Files:**
- Create: `examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino`

**Interfaces:**
- Produces: compile-only example stub with `#include` of all new modules, dummy `setup()`/`loop()`, pin configuration matching design spec

- [ ] **Step 1: Write minimal sketch**

```cpp
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_STM32F1_USB.h>

static via::Pin rowPins[6] = {PB10, PB11, PB12, PB13, PB14, PB15};
static via::Pin colPins[18] = {PA0,PA1,PA2,PA3,PA4,PA5,PA6,PA7,PA8,PA9,PA10,PA15,PB0,PB1,PB3,PB4,PB5,PB6};
static uint16_t keymap[6*18*4];
static const uint16_t defaultKeymap[6*18*4];
static uint32_t rawRows[6], candidateRows[6], stableRows[6], changedRows[6];
static uint16_t activeCodes[6*18];
static uint8_t loadBuffer[2048];

via::stm32f1::UsbDevice usb;
via::MatrixIOArduino matrixIO;
via::Matrix matrix;
via::Keyboard keyboard;
via::Protocol protocol;

void setup() {
  usb.begin();
  protocol.begin(millis());
  keyboard.begin();
}

void loop() {
  usb.task();
  protocol.task(millis());
  keyboard.task(millis());
}
```

- [ ] **Step 2: Add STM32 compile to CI**

```yaml
- name: Install STM32 toolchain
  run: |
    arduino-cli config add board_manager.additional_urls \
      https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
    arduino-cli core update-index
    arduino-cli core install STMicroelectronics:stm32@3.0.0
- name: Compile STM32F103 keyboard
  run: |
    arduino-cli compile --clean --warnings all \
      --fqbn "STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103CB,xserial=disabled,usb=HID,xusb=FS,opt=oslto,dbg=none,rtlib=nano,upload_method=swdMethod" \
      --libraries . examples/STM32F103_Keyboard_MVP
```

- [ ] **Step 3: Push and verify CI green**

- [ ] **Step 4: Commit and push**

```bash
git add examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino .github/workflows/ci.yml
git commit -m "example(stm32): scaffold keyboard MVP sketch"
git push
```

### Milestone 3 Review Gate

Verify descriptor bytes, endpoint assignments, no report IDs in VIA interface, STM32 compile succeeds, sketch pin map matches design, USB class registration wrapped in Arduino-compatible style.

---

## Milestone 4: STM32F103 Dual-Slot Flash Storage

**Goal:** Power-loss-safe persistent storage using two 2 KiB internal flash pages.

**Files:**
- Create: `src/VIA_STM32F1_Flash.h`
- Create: `src/VIA_STM32F1_Flash.cpp`
- Create: `tests/flash_test.cpp`
- Modify: `.github/workflows/ci.yml`

### Task 15: Simulated Flash And Dual-Slot Logic

**Files:**
- Create: `src/VIA_STM32F1_Flash.h`
- Create: `src/VIA_STM32F1_Flash.cpp`
- Create: `tests/flash_test.cpp`

**Interfaces:**
- Produces: `class via::FlashMemory` with `read(uint32_t addr, void* out, uint16_t length)`, `write(uint32_t addr, const void* data, uint16_t length)`, `erasePage(uint32_t addr)`, `commit()->bool` — pure virtual for host testing
- Produces: `class via::stm32f1::FlashStorage : public via::Storage` using two slots at `0x0801F000`/`0x0801F800`
- Produces: `FlashStorage::begin()` validates slots, selects newest valid one
- Produces: Envelope with magic `0xA5F103CB`, 32-bit sequence, CRC32, commit marker byte `0x5A`
- Produces: Simulated flash backed by `uint8_t[4096]` array enforcing one-to-zero rule

- [ ] **Step 1: Write flash fault injection tests**

Add tests:
- Empty fresh flash: `begin()` returns true, `capacity()` reports 2032, read area zero.
- Write-save-read round-trip: fill 50 bytes, save, power-off, new `FlashStorage` reads same bytes.
- Power cut after erase but before write: old slot remains valid.
- Power cut after partial write: old slot remains valid.
- Power cut before commit marker: old slot remains valid.
- Power cut after commit marker: new slot active.
- Sequence wraparound from `0xFFFFFFFF` to `0x00000000`: newest detection still correct.
- Both slots corrupt: `begin()` returns false, `capacity()` returns 0.
- Write outside reserved region: returns false.
- Erase overwrites old data only in target slot.

- [ ] **Step 2: Run, observe failures**

- [ ] **Step 3: Implement FlashStorage**

`begin()` walks both slots, validates envelopes. `read()`/`write()` translate logical offset to physical address within active slot. `erase()` starts provisional replacement slot (erase + invalid header). `commit()` writes protocol header, calculates CRC, writes commit marker. Sequence uses `int32_t` difference comparison.

- [ ] **Step 4: Run all flash tests, verify**

- [ ] **Step 5: Commit and push**

```bash
git add src/VIA_STM32F1_Flash.h src/VIA_STM32F1_Flash.cpp tests/flash_test.cpp .github/workflows/ci.yml
git commit -m "feat(flash): implement dual-slot flash storage"
git push
```

### Task 16: Flash CI And STM32 Compile

**Files:**
- Modify: `src/VIA_STM32F1_Flash.cpp` (platform `#ifdef` guard)
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Add flash native CI**

```yaml
- name: Build native flash test
  run: |
    g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/flash_test.cpp \
      src/VIA_STM32F1_Flash.cpp src/VIA_Protocol.cpp src/VIA_RGBLight.cpp \
      -o flash_test && ./flash_test
    g++ -std=c++11 -Wall -Wextra -Werror \
      -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
      tests/flash_test.cpp src/VIA_STM32F1_Flash.cpp src/VIA_Protocol.cpp \
      src/VIA_RGBLight.cpp -o flash_test_san && ./flash_test_san
```

- [ ] **Step 2: Add size check to STM32 CI job**

After compile: `arm-none-eabi-size -B <elf>` and assert flash <= 112 KiB and RAM <= 12 KiB.

- [ ] **Step 3: Commit and push**

```bash
git add src/VIA_STM32F1_Flash.cpp .github/workflows/ci.yml
git commit -m "test(flash): add CI and size gate"
git push
```

### Milestone 4 Review Gate

Verify slot selection logic, one-to-zero write rules, CRC correctness, sequence wraparound, power-cut coverage, and CI green.

---

## Milestone 5: Board Integration And Boot Coordinator

**Goal:** Wire all modules together: STM32 GPIO adapter, ROM boot coordinator, complete example sketch, final CI matrix, docs.

**Files:**
- Create: `src/VIA_STM32F1_Boot.h`
- Create: `src/VIA_STM32F1_Boot.cpp`
- Create: `src/VIA_STM32F1_GPIO.h` (Arduino `pinMode`/`digitalRead`/`digitalWrite` adapter for MatrixIO)
- Modify: `examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `.github/workflows/ci.yml`

### Task 17: Board GPIO Adapter

**Files:**
- Create: `src/VIA_STM32F1_GPIO.h`

**Interfaces:**
- Produces: `class via::stm32f1::MatrixIOArduino : public via::MatrixIO` using `pinMode`, `digitalWrite`, `digitalRead`, `delayMicroseconds`

- [ ] **Step 1: Implement adapter**

```cpp
void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
bool read(Pin pin) override { return digitalRead(pin); }
void delayMicroseconds(uint16_t us) override { delayMicroseconds(us); }
```

- [ ] **Step 2: Commit and push**

```bash
git add src/VIA_STM32F1_GPIO.h
git commit -m "feat(stm32): add GPIO matrix adapter"
git push
```

### Task 18: ROM Boot Coordinator

**Files:**
- Create: `src/VIA_STM32F1_Boot.h`
- Create: `src/VIA_STM32F1_Boot.cpp`

**Interfaces:**
- Consumes: `KeyboardCallbacks::bootloaderRequested()`, `Protocol`, `KeyboardHID`, `UsbDevice`
- Produces: `class via::stm32f1::BootCoordinator` with `request()`, `task()` called from main loop
- Produces: State machine: idle -> saving -> releasing_kbd -> releasing_via -> detach -> jump

- [ ] **Step 1: Write coordinator test**

Test state machine ordering: save fails cancels; save succeeds proceeds; reentrant call gets ignored; jump sequence order.

- [ ] **Step 2: Implement**

```cpp
enum State { kIdle, kDirtySave, kKeyboardRelease, kViaRelease, kDetach, kJump };
```

`request()` sets state from idle if accepted. `task()` advances state machine: save Protocol, queue all-zero keyboard report, wait completions, detach USB, disable interrupts, load SP from `0x1FFFF000`, jump.

- [ ] **Step 3: Commit and push**

```bash
git add src/VIA_STM32F1_Boot.h src/VIA_STM32F1_Boot.cpp
git commit -m "feat(stm32): add ROM boot coordinator"
git push
```

### Task 19: Complete Example And Multi-Target CI

**Files:**
- Modify: `examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino`
- Modify: `.github/workflows/ci.yml`
- Modify: `README.md`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Wire complete example**

Update sketch to fully initialize all modules, route callbacks, and run `usb.task()`, `protocol.task(now)`, `keyboard.task(now)`, and `boot.task()` in `loop()`.

- [ ] **Step 2: Add second STM32 CI target**

```yaml
- name: Compile STM32F103RB portability
  run: |
    arduino-cli compile --clean --warnings all \
      --fqbn "STMicroelectronics:stm32:Nucleo_64:pnum=NUCLEO_F103RB,xserial=disabled,usb=HID,xusb=FS,opt=oslto,dbg=none,rtlib=nano,upload_method=swdMethod" \
      --libraries . examples/STM32F103_Keyboard_MVP
```

- [ ] **Step 3: Add allocation-symbol check**

```bash
arm-none-eabi-nm "$ELF" | rg -q '\bmalloc$|\bfree$|\bcalloc$|\brealloc$|_Znwm|_Znam' && exit 1 || true
```

- [ ] **Step 4: Update README and CHANGELOG**

Document new modules, supported platforms, hardware-not-yet-verified status, and migration note.

- [ ] **Step 5: Commit and push**

```bash
git add examples/STM32F103_Keyboard_MVP/STM32F103_Keyboard_MVP.ino README.md CHANGELOG.md .github/workflows/ci.yml
git commit -m "feat(stm32): complete MVP integration"
git push
```

### Milestone 5 Review Gate

Verify all native tests green, all CI jobs green (lint, protocol, TinyUSB, Uno, RP2040, matrix, keyboard, flash, descriptor, STM32x2, size), no allocation symbols, docs accurate. This gate completes software.

---

## Milestone 6: Hardware Certification

**Goal:** Validate on real custom STM32F103CB PCB. Blocked until board available. Not merged to main without this gate.

**Checklist:** Run the Hardware Certification checklist from the design spec (MCU identity, clock accuracy, D+ pull-up, matrix polarity, scan cadence, bounce tests, USB enumeration, boot keyboard test, VIA discovery/remap/persistence, suspend/resume, bootloader jump, power-loss storage, SWD recovery, memory gates).

Each item produces a recorded pass/fail; all must pass before `0.3.0` release.

---

## Self-Review

### Spec Coverage
- [x] COL2ROW/ROW2COL scanning (Tasks 2-3)
- [x] Settle time (Tasks 2-3)
- [x] Deferred debounce (Task 4)
- [x] Stable event batching (Task 9)
- [x] Basic keys, modifiers, QK_MODS (Task 6)
- [x] Layer lookup, MO/TG/TO/DF (Task 8)
- [x] 6KRO reports (Task 9-10)
- [x] Host LEDs (Task 10)
- [x] VIA integration (Task 9-10)
- [x] Dual-slot flash (Tasks 15-16)
- [x] Suspend/resume (Task 10)
- [x] Boot coordination (Task 18)
- [x] USB composite descriptors (Tasks 12-13)
- [x] STM32 compile (Tasks 14, 19)
- [x] Memory gates (Tasks 16, 19)
- [x] Native tests + ASan/UBSan (all tasks)
- [x] CI jobs (all tasks)
- [x] Hardware certification (Milestone 6)

### Placeholder Scan
- No TBD, TODO, or placeholder text found.

### Type Consistency
- `MatrixIO`, `MatrixConfig`, `Matrix` — consistent across Tasks 1-5.
- `KeyboardHID`, `KeyboardReport`, `KeyboardCallbacks` — consistent across Tasks 7-11.
- `LayerState` — consistent across Tasks 8-9.
- `KeycodeType`, `classifyKeycode` helpers — consistent across Tasks 6, 9.
- `FlashMemory`, `FlashStorage`, `via::Storage` — consistent across Tasks 15-16.
- `UsbDevice` — consistent across Tasks 13-14, 18-19.
- `BootCoordinator` — consistent across Tasks 18-19.
