# VIA-Arduino 0.2.0 Protocol Findings

This document records the audited scope implemented by 0.2.0. It is not a
claim of full QMK firmware or broad hardware compatibility.

## Matrix State

`Callbacks::matrixRow(uint8_t)` returns `uint32_t`. Command `0x02/0x03` packs
each row into 1-4 big-endian bytes according to `Config::columns`, using the
28-byte response payload. Disclosure defaults off; without
`matrixStateEnabled`, the command returns zero state and does not call the
callback.

## Layout, Encoders, And Indication

- Layout options are a persistent 32-bit value exposed by `0x02/0x02` and
  `0x03/0x02`; changes call `layoutOptionsChanged(uint32_t)`.
- Encoder maps hold two 16-bit keycodes per encoder per layer. Commands `0x14`
  and `0x15` read and write them; keymap reset restores their defaults.
- Device indication forwards the complete byte to
  `deviceIndication(uint8_t)`, rather than reducing it to a boolean.

## Persistence

Storage records contain a 12-byte header, keymap, encoder map, macros, layout
options, and up to 16 custom-state bytes. Reads validate metadata and CRC before
committing staged state. Every storage-backed `Protocol` requires a distinct,
caller-owned `loadBuffer` sized by `requiredLoadBufferSize()`.

## Sensitive Operations

Matrix disclosure, EEPROM reset (`0x0A`), and bootloader jump (`0x0B`) are
independent opt-ins. Bootloader callbacks are deferred until `task()` sends the
response successfully; direct `process()` never invokes the callback.

## Platform Evidence

Native protocol tests, an Arduino Uno core-only example, and the Raspberry Pi
Pico RP2040 TinyUSB example compile in CI. No other platform is currently
claimed as verified, and CI compilation is not hardware validation.
