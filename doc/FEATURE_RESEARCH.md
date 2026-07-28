# VIA-Arduino Feature Research & Bug Report

## 1. CRITICAL BUG: Switch Matrix Tester Limited to 8 Columns
**Status:** Bug Found
**Description:** In `VIA_Protocol.cpp`, the handler for `id_get_keyboard_value` -> `switch matrix state` (0x02, 0x03) hardcodes a response that maps one byte per row, returning only two rows at most. Furthermore, `Callbacks::matrixRow` returns `uint8_t`. This fundamentally restricts any keyboard using this library to a maximum of 8 columns. Standard QMK keyboards typically have 14-16 columns, which would cause the VIA key tester to silently drop or falsely report key presses for columns 9-16.
**Proposed Fix:** 
1. Upgrade `Callbacks::matrixRow` to return `uint32_t` (supporting up to 32 columns).
2. Rewrite the matrix serialization logic to dynamically pack 1, 2, 3, or 4 bytes per row depending on `config_.columns`, fully utilizing the 28-byte payload space as standard QMK firmware does.

## 2. Missing Feature: Bootloader Jump
**Status:** Not Implemented
**Description:** The library does not implement the standard VIA command `id_bootloader_jump` (0x0B). This prevents the user from entering bootloader mode via the VIA Configurator UI.
**Proposed Addition:** Add a `bootloaderJump()` method to `Callbacks` and implement command `0x0B` to invoke it.
