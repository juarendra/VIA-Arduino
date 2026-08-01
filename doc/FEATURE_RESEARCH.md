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
caller-owned `loadBuffer` sized by `requiredLoadBufferSize()`. The core rejects
overlap with mutable or default keymap and encoder-map buffers, and with the
macro buffer. The caller must keep `CustomValue` handler-owned active state
separate because the core cannot inspect it. Staged custom bytes pass through
non-mutating `CustomValue::validateState()` before direct-load publication or
factory-reset storage mutation. Handlers that can reject bytes must override
validation; acceptance guarantees `loadState()` publishes those same bytes.
Factory reset stages defaults in the same workspace and publishes live state
and callbacks only after durable storage commit. Successful stored/default
loads and resets notify the final layout value; failed direct loads and resets
do not notify or mutate live state.

The schema changed from version 1 to version 2. Version 1 records from 0.1.0
are rejected, causing `begin()` to perform its one-time startup fallback to
configured built-in defaults. There is no automatic migration or save; users
must reconfigure and save a version 2 record.

## Sensitive Operations

Matrix disclosure, EEPROM reset (`0x0A`), and bootloader jump (`0x0B`) are
independent opt-ins. Bootloader acceptance requires a callback and persists
dirty state first. Callbacks are deferred until the transport reports the
accepted response complete; direct `process()` saves or rejects but never jumps.

## Platform Evidence

Native protocol tests, an Arduino Uno core-only example, and the Raspberry Pi
Pico RP2040 TinyUSB example compile in CI. No other platform is currently
claimed as verified, and CI compilation is not hardware validation.
