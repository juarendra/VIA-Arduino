# VIA Protocol Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make VIA-Arduino's protocol v13 behavior safe, testable, and wire-compatible with current QMK/VIA before adding keyboard-runtime features.

**Architecture:** Keep `via::Protocol` as the portable protocol core. Extend its existing fixed-size configuration and callback contracts only where protocol v13 requires state or authorization. Keep platform behavior in adapters and avoid heap allocation.

**Tech Stack:** C++11, Arduino library format, native `assert` tests, Adafruit TinyUSB adapter, GitHub Actions, Arduino CLI.

## Global Constraints

- Keep the project MIT licensed; do not copy GPL QMK implementation code.
- Target VIA protocol version `0x000D` and QMK keycodes ABI `0.0.8`.
- Preserve the fixed 32-byte Raw HID packet and dedicated usage page `0xFF60`, usage `0x61`.
- Keep C++11 compatibility and avoid heap allocation.
- New configuration fields must be appended so existing aggregate initializers remain source-compatible and default disabled or zero.
- Matrix-state disclosure, EEPROM reset, and bootloader jump must be opt-in.
- Every production behavior change must first have a failing native test.
- Push the feature branch after each reviewed task; do not push directly to `main`.

---

### Task 1: Matrix Packing And Reset Isolation

**Files:**
- Modify: `tests/protocol_test.cpp`
- Modify: `src/VIA_Protocol.cpp`

**Interfaces:**
- Consumes: `Callbacks::matrixRow(uint8_t) -> uint32_t`
- Produces: protocol command `0x02/0x03` with 28-byte, width-aware, big-endian matrix rows
- Produces: command `0x06` resetting keymap only and command `0x10` resetting macros only

- [ ] **Step 1: Add failing golden matrix tests**

Add a callback returning fixed row masks and assertions covering 8, 16, 24, and 32 columns. Exact expected bytes:

```cpp
// 16 columns, row mask 0x1234.
assert(packet[3] == 0x12);
assert(packet[4] == 0x34);

// 32 columns, row mask 0x12345678.
assert(packet[3] == 0x12);
assert(packet[4] == 0x34);
assert(packet[5] == 0x56);
assert(packet[6] == 0x78);
```

Also assert bytes outside the 28-byte matrix payload are zero and row offsets do not wrap from `255` to row `0`.

- [ ] **Step 2: Add failing reset-isolation tests**

```cpp
// 0x06 restores the keymap but preserves macro bytes.
assert(keyboard.process(packet, 30));
assert(keyboard.keycode(0, 0, 0) == defaults[0]);
assert(macros[0] == 'v');

// 0x10 clears macros without changing keymap.
assert(keyboard.process(packet, 31));
assert(macros[0] == 0);
assert(keyboard.keycode(0, 0, 0) == defaults[0]);
```

- [ ] **Step 3: Run tests and confirm expected failures**

Run:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/protocol_test.cpp src/VIA_Protocol.cpp src/VIA_RGBLight.cpp -o protocol_test && ./protocol_test
```

Expected: matrix byte-order or reset-isolation assertion fails.

- [ ] **Step 4: Implement minimal protocol fixes**

Serialize each matrix row from its highest present byte to its lowest. Use exactly `28 / bytesPerRow` rows. Split keymap and macro reset logic; command `0x06` must not clear macros.

- [ ] **Step 5: Run native test and Arduino compile checks**

Run the native command above. Then run:

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries . examples/Protocol_Self_Test
```

Expected: both commands exit `0`.

- [ ] **Step 6: Commit and push**

```bash
git add tests/protocol_test.cpp src/VIA_Protocol.cpp
git commit -m "fix(protocol): correct matrix and reset behavior"
git push
```

### Task 2: Layout, Encoder, And Indication Semantics

**Files:**
- Modify: `tests/protocol_test.cpp`
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`

**Interfaces:**
- Consumes: appended `Config` fields for default layout options, encoder count, mutable/default encoder maps
- Produces: persistent 32-bit layout options using command `0x02/0x02` and `0x03/0x02`
- Produces: encoder commands `0x14` and `0x15` with `[layer, encoder, clockwise, keycode_hi, keycode_lo]`
- Produces: `Callbacks::deviceIndication(uint8_t value)` and `Callbacks::layoutOptionsChanged(uint32_t value)`

- [ ] **Step 1: Add failing layout tests**

Set layout options to `0x12345678`, read them back, and assert bytes `2..5` are `12 34 56 78`. Assert `layoutOptionsChanged(0x12345678)` is called once.

- [ ] **Step 2: Add failing encoder tests**

Use one encoder, two directions, and two layers. Assert command `0x15` writes a 16-bit keycode, command `0x14` returns it big-endian, invalid layer/encoder/direction returns `0xFF`, and command `0x06` restores default encoder maps.

- [ ] **Step 3: Add failing indication test**

Send command `0x03/0x05` with value `4` and assert the callback receives `4`, not boolean `true`.

- [ ] **Step 4: Run tests and confirm expected failures**

Run the native test command from Task 1.

Expected: compilation fails because required fields/callbacks are absent or assertions fail because commands are unsupported.

- [ ] **Step 5: Implement minimal state and command handling**

Append configuration fields, add protocol state/accessors, add encoder bounds checks, and include layout/encoder data in persistence sizing and reset behavior. Existing ten-field aggregate initializers must compile unchanged.

- [ ] **Step 6: Run native and Arduino compile checks**

Run Task 1 native and Uno commands.

Expected: both exit `0`.

- [ ] **Step 7: Commit and push**

```bash
git add tests/protocol_test.cpp src/VIA_Protocol.h src/VIA_Protocol.cpp
git commit -m "feat(protocol): add layout and encoder parity"
git push
```

### Task 3: Custom-Value Save Semantics

**Files:**
- Modify: `tests/protocol_test.cpp`
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`
- Modify: `src/VIA_RGBLight.h`
- Modify: `src/VIA_RGBLight.cpp`

**Interfaces:**
- Produces: `CustomValue::save(uint8_t packet[kPacketSize]) -> bool`
- Produces: generic command `0x09` routing without channel hardcoding
- Produces: public `kMaxCustomStateSize = 16`

- [ ] **Step 1: Add failing custom-save tests**

Create a test custom handler for channel `7`. Assert commands `0x07`, `0x08`, and `0x09` reach its set/get/save methods. Assert unsupported channels return `0xFF` through the handler, not protocol hardcoding.

- [ ] **Step 2: Add failing custom-size validation test**

Create a handler reporting `stateSize() == kMaxCustomStateSize + 1` and assert `Protocol::begin()` returns false without reading or writing storage.

- [ ] **Step 3: Run tests and confirm expected failures**

Run the native test command from Task 1.

Expected: compilation fails because `save()` and the public limit are absent.

- [ ] **Step 4: Implement generic save routing and explicit size contract**

Add the virtual save hook, route command `0x09` to it, and persist only after the handler accepts the packet. Validate custom state size during `begin()`.

- [ ] **Step 5: Run native and Arduino compile checks**

Run Task 1 native and Uno commands.

Expected: both exit `0`.

- [ ] **Step 6: Commit and push**

```bash
git add tests/protocol_test.cpp src/VIA_Protocol.h src/VIA_Protocol.cpp src/VIA_RGBLight.h src/VIA_RGBLight.cpp
git commit -m "fix(protocol): route custom-value saves"
git push
```

### Task 4: Persistence Integrity And Bounds

**Files:**
- Modify: `tests/protocol_test.cpp`
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`

**Interfaces:**
- Produces: checked state-size validation during `begin()`
- Produces: CRC validation before active keymap, encoder, macro, layout, or custom state mutation
- Produces: bounded macro and keymap bulk operations without 16-bit wraparound

- [ ] **Step 1: Add failing corrupt-load test**

Save a valid record, corrupt one payload byte, initialize active buffers with sentinels, call `load()`, and assert it returns false while every active sentinel remains unchanged.

- [ ] **Step 2: Add failing state-size tests**

Use dimensions whose keymap calculation exceeds `UINT16_MAX` or platform `SIZE_MAX`. Assert `begin()` returns false before storage access.

- [ ] **Step 3: Add failing offset and zero-length tests**

For macro/keymap offsets `0xFFFF`, assert GET returns zero-filled payload, SET changes no state, and no dirty callback occurs. Assert zero-length writes do not mark dirty.

- [ ] **Step 4: Run tests and confirm expected failures**

Run the native test command from Task 1.

Expected: corrupt-load or offset assertions fail.

- [ ] **Step 5: Implement checked arithmetic and two-pass load validation**

Compute state sizes in `uint32_t`, reject unrepresentable records, stream storage once to validate CRC, then read validated state into active buffers. Serialize custom state once per save so payload and CRC use identical bytes.

- [ ] **Step 6: Run native and Arduino compile checks**

Run Task 1 native and Uno commands.

Expected: both exit `0` with warnings treated as errors in native build.

- [ ] **Step 7: Commit and push**

```bash
git add tests/protocol_test.cpp src/VIA_Protocol.h src/VIA_Protocol.cpp
git commit -m "fix(storage): validate state before mutation"
git push
```

### Task 5: Security And Reliable Response Delivery

**Files:**
- Modify: `tests/protocol_test.cpp`
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`
- Modify: `src/VIA_TinyUSB_RawHID.cpp`

**Interfaces:**
- Consumes: appended `Config` booleans `matrixStateEnabled`, `eepromResetEnabled`, and `bootloaderEnabled`
- Produces: default-deny sensitive commands
- Produces: one pending response retained until `Transport::send()` succeeds
- Produces: bootloader callback only after its response is sent successfully

- [ ] **Step 1: Add failing security-policy tests**

Assert matrix-state response is zero by default, EEPROM reset returns `0xFF` by default, and bootloader command returns `0xFF` by default. Enable each option independently and assert only that behavior becomes available.

- [ ] **Step 2: Add failing send-retry test**

Use a transport whose first send fails and second succeeds. Assert the same response packet is retried, no second request is consumed meanwhile, and mutating command effects occur once.

- [ ] **Step 3: Add failing deferred-bootloader test**

Assert bootloader callback count remains zero after processing and failed send, then becomes one only after successful response send.

- [ ] **Step 4: Run tests and confirm expected failures**

Run the native test command from Task 1.

Expected: security or retry assertions fail.

- [ ] **Step 5: Implement opt-in policy and pending-response state**

Append flags to `Config`, store one response in `Protocol`, retry it before receiving another request, and invoke deferred bootloader action only after successful send.

- [ ] **Step 6: Accept valid TinyUSB OUT callback types without overwrite**

Accept endpoint callback report type `0` or `HID_REPORT_TYPE_OUTPUT`. Ignore a new packet while `rxReady_` is already true.

- [ ] **Step 7: Run native, Uno, and RP2040 checks**

Run Task 1 native and Uno commands, then:

```bash
arduino-cli compile --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb --libraries . examples/RP2040_VIA_RawHID
```

Expected: all exit `0`.

- [ ] **Step 8: Commit and push**

```bash
git add tests/protocol_test.cpp src/VIA_Protocol.h src/VIA_Protocol.cpp src/VIA_TinyUSB_RawHID.cpp
git commit -m "fix(protocol): gate sensitive commands"
git push
```

### Task 6: Release Truth And CI

**Files:**
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `doc/FEATURE_RESEARCH.md`
- Modify: `docs/PORTING.md`
- Modify: `docs/ARDUINO_LIBRARY_MANAGER.md`
- Modify: `library.properties`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: accurate `0.2.0` release metadata and capability claims
- Produces: CI linting an already-registered Arduino library in update mode
- Produces: documented secure defaults and exact supported/tested platform evidence

- [ ] **Step 1: Correct version and positioning**

Set metadata and docs to `0.2.0`. Position the package as a portable VIA protocol core with RP2040 compile-tested adapters, not a complete keyboard firmware or verified 50+ platform implementation.

- [ ] **Step 2: Replace nonexistent API examples**

Use actual `via::Protocol`, `process()`, `task()`, `load()`, and `save()` APIs. State explicitly that matrix scanning and QMK keycode execution remain application responsibilities in this release.

- [ ] **Step 3: Document breaking and security changes**

Document `matrixRow()` width, indication callback value, appended configuration fields, matrix disclosure default, EEPROM reset opt-in, bootloader opt-in, layout options, and encoder maps.

- [ ] **Step 4: Fix Library Manager lint mode**

Change `library-manager: submit` to `library-manager: update`. Remove duplicated metadata prose while preserving the registered library name.

- [ ] **Step 5: Run documentation and full CI-equivalent checks**

Run native, Uno, and RP2040 commands from earlier tasks plus:

```bash
arduino-lint --compliance specification --project-type library --library-manager update
```

Expected: all commands exit `0`.

- [ ] **Step 6: Commit and push**

```bash
git add README.md CHANGELOG.md doc/FEATURE_RESEARCH.md docs/PORTING.md docs/ARDUINO_LIBRARY_MANAGER.md library.properties .github/workflows/ci.yml
git commit -m "docs: align 0.2 capabilities and release"
git push
```

### Task 7: Whole-Branch Verification

**Files:**
- Test only; no planned production changes

**Interfaces:**
- Consumes: Tasks 1-6
- Produces: reviewed branch ready for pull request

- [ ] **Step 1: Run complete local verification**

Run all native, Uno, RP2040, and Arduino Lint commands from this plan.

- [ ] **Step 2: Verify repository state**

```bash
git status --short --branch
git diff --check origin/main...HEAD
git log --oneline origin/main..HEAD
```

Expected: clean worktree, no whitespace errors, only intended commits.

- [ ] **Step 3: Review the full branch**

Review `origin/main...HEAD` for protocol compatibility, security regressions, memory safety, API breakage, missing tests, and documentation accuracy. Fix all Critical and Important findings with tests before proceeding.

- [ ] **Step 4: Push final branch state**

```bash
git push
```

- [ ] **Step 5: Report branch and CI evidence**

Return the remote branch URL, commit list, local command results, GitHub Actions result, known limitations, and next planned branch `feat/0.3-stm32f103-keyboard-mvp`.
