# Protocol Correctness Final Fixes Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix final AVR decoding, transactional reset, layout notification, workspace alias, and reset-test findings without changing public source compatibility.

**Architecture:** Keep `Protocol` and existing caller-owned `loadBuffer`. Stage factory defaults as a complete payload, write and commit that payload, then publish live state and callbacks. Keep direct loads staged until validation succeeds.

**Tech Stack:** C++11, fixed 32-byte packets, caller-owned static storage, native `assert` tests, WSL `g++`.

## Global Constraints

- MIT clean; no QMK implementation copying.
- No heap allocation.
- Preserve existing aggregate initialization and public APIs.
- Preserve command `0x0A` and default-deny reset policy.
- Push tests-only RED before production fixes.
- Run native WSL verification; rely on GitHub CI for Uno, RP2040, and lint.

---

### Task 1: Focused Regression Tests

**Files:**
- Modify: `tests/protocol_test.cpp`

**Interfaces:**
- Exercises existing packet commands, lifecycle methods, callbacks, `Storage`, and `CustomValue`.
- Adds no production interface.

- [ ] Add high-byte packet tests using values and offsets at or above `0x8000` for point keycode, macro get/set, bulk keymap get/set, and encoder keycode.
- [ ] Add reset failure-injection storage covering erase, write, and commit failures; assert keymap, encoder map, full macro buffer, layout, custom state, dirty state, and callbacks remain unchanged.
- [ ] Add lifecycle notification tests for stored `begin()`, direct `load()`, default fallback, successful reset, and failed load/reset.
- [ ] Add immutable default-keymap/default-encoder overlap cases to load-workspace validation.
- [ ] Strengthen `0x06` and `0x10` checks to compare complete macro buffers.
- [ ] Run WSL native suite and verify expected assertion failures are caused by current production behavior.
- [ ] Commit and push tests-only RED.

### Task 2: Minimal Production Fixes

**Files:**
- Modify: `src/VIA_Protocol.cpp`
- Modify only if needed: `src/VIA_Protocol.h`

**Interfaces:**
- Keeps all public signatures unchanged.
- Uses `Config::loadBuffer` as factory-reset staging storage.

- [ ] Cast every packet byte used as the left operand of `<< 8` to `uint16_t` before shifting.
- [ ] Reject `loadBuffer` overlap with mutable and immutable keymap/encoder ranges plus macros.
- [ ] Notify `layoutOptionsChanged()` exactly once after successful persisted/default load publication, and never after failed load.
- [ ] Build reset defaults in `loadBuffer`; write header and staged payload; commit before mutating live state or invoking callbacks.
- [ ] On reset success, publish staged built-ins, reset custom state, clear dirty state, then notify final layout and changed state once.
- [ ] Run WSL native suite to GREEN.
- [ ] Commit and push production GREEN.

### Task 3: Contracts, Report, And Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/PORTING.md`
- Modify: `doc/FEATURE_RESEARCH.md`
- Create: `.superpowers/sdd/2026-08-01-protocol-correctness/final-fix-report.md`

**Interfaces:**
- Documents transactional factory reset and layout lifecycle notifications.

- [ ] Update only changed load/reset contracts and immutable-default workspace exclusions.
- [ ] Run fresh WSL native suite, `git diff --check`, and inspect complete diff/status/log.
- [ ] Write RED/GREEN evidence, design, files, commits, pushes, and concerns to requested report path.
- [ ] Commit and push documentation/report.
- [ ] Check GitHub Actions for pushed HEAD and record available result.

### Task 8: Non-Mutating Custom State Validation

**Files:**
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`
- Modify: `tests/protocol_test.cpp`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/PORTING.md`
- Modify: `doc/FEATURE_RESEARCH.md`
- Create: `.superpowers/sdd/2026-08-01-protocol-correctness/task-8-report.md`

**Interfaces:**
- Adds source-compatible virtual `CustomValue::validateState(const uint8_t*, size_t) const`.
- Validation is non-mutating and defaults to exact `stateSize()` length.
- A successful validation guarantees `loadState()` publishes the same bytes; a failed
  `loadState()` remains non-mutating.

- [ ] Add and push failing direct-load and factory-reset validation tests before production code.
- [ ] Validate staged custom bytes before direct-load publication.
- [ ] Validate custom reset bytes before storage erase, write, or commit.
- [ ] Publish custom reset state without a fallible branch after durable commit.
- [ ] Document migration for custom handlers that reject persisted state.
- [ ] Run WSL warnings-as-errors and ASan/UBSan suites, push, and verify GitHub CI.
- [ ] Record RED/GREEN evidence, commits, CI, and concerns in the Task 8 report.

### Task 9: Boot Response Transfer Completion

**Files:**
- Modify: `src/VIA_Protocol.h`
- Modify: `src/VIA_Protocol.cpp`
- Modify: `src/VIA_TinyUSB_RawHID.h`
- Modify: `src/VIA_TinyUSB_RawHID.cpp`
- Modify: `tests/protocol_test.cpp`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/PORTING.md`
- Modify: `doc/FEATURE_RESEARCH.md`
- Create: `.superpowers/sdd/2026-08-01-protocol-correctness/task-9-report.md`

**Interfaces:**
- Adds source-compatible virtual `Transport::sendComplete()` with synchronous
  completion by default.
- TinyUSB reports completion by polling `hid_.ready()` only after `send()` has
  accepted the response; no global completion callback or timeout is used.
- Boot command acceptance requires enabled policy, a callback, and successful
  persistence of dirty state.

- [ ] Add and push deterministic failing completion, retry, ordering, and boot
  persistence-gate tests before production code.
- [ ] Distinguish an unsent response from an accepted in-flight boot response;
  never resend an accepted response and clear boot state before jumping once.
- [ ] Override TinyUSB completion with endpoint readiness polling and preserve
  existing synchronous adapters through the default implementation.
- [ ] Reject disabled or callback-less boot requests and dirty-state save
  failures; keep clean no-storage requests bootable and direct processing
  non-jumping.
- [ ] Move `validateState()` additions and migration notes from `[Unreleased]`
  into `0.2.0`.
- [ ] Run WSL warnings-as-errors and ASan/UBSan suites, push, and verify complete
  GitHub CI including the RP2040 TinyUSB reference compile.
- [ ] Record root cause, RED/GREEN evidence, commits, CI, and concerns in the
  Task 9 report.

### Task 10: Final Integration Lifetimes

**Files:**
- Modify: `src/VIA_Protocol.cpp`
- Modify: `src/VIA_TinyUSB_RawHID.h`
- Modify: `src/VIA_TinyUSB_RawHID.cpp`
- Modify: `tests/protocol_test.cpp`
- Add: `tests/fakes/Adafruit_TinyUSB.h`
- Add: `tests/tinyusb_rawhid_test.cpp`
- Modify: `.github/workflows/ci.yml`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/PORTING.md`
- Create: `.superpowers/sdd/2026-08-01-protocol-correctness/task-10-report.md`

**Interfaces:**
- Keeps mandatory caller-owned full `Config::loadBuffer` and all public method
  signatures source-compatible.
- Gives `RawHID` an owning destructor while preserving one instance per USB
  device and provisional callback registration during `begin()`.

- [ ] Add and push failing tests for changing storage reads, successful and
  failed load dirty state, failed TinyUSB initialization replacement, and owner
  destruction replacement before production code.
- [ ] Read each payload once into `loadBuffer`; check CRC and custom state on
  those exact bytes before publishing live state or callbacks.
- [ ] Clear dirty state only after successful load state/callback publication;
  preserve it on every failure.
- [ ] Clear `RawHID::active_` only from its owner destructor and roll back a
  provisional owner when `hid_.begin()` fails.
- [ ] Document staged-load and one-instance/device-lifetime contracts.
- [ ] Run WSL C++11 warnings-as-errors and ASan/UBSan suites, compile RP2040,
  push periodically, and verify complete GitHub CI.
- [ ] Record status, RED/GREEN/CI evidence, commits, and concerns in Task 10
  report.
