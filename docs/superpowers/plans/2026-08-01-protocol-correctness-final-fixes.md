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
