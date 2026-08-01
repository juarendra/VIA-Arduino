# STM32F103 Keyboard MVP Design

## Status

Approved design for planning. This document defines phase `0.3` only. It does
not claim hardware verification because the reference PCB is not yet available.

## Goal

Turn VIA-Arduino from a VIA protocol core into a usable wired keyboard firmware
library on a custom STM32F103CB board while preserving these project properties:

- MIT-licensed project code; no QMK GPL source copied.
- Portable C++11 behavior core.
- No heap allocation.
- Deterministic native tests.
- Existing protocol, Uno, and RP2040 compatibility remains green.
- QMK keycode ABI `0.0.8` semantics for the explicitly supported subset.

Phase `0.3` reaches software-complete status when native tests, STM32 builds,
descriptor snapshots, memory gates, and documentation pass. A final `0.3.0`
hardware-verified release remains blocked until the hardware certification
section passes on the custom board.

## Reference Platform

- MCU: genuine STM32F103CBT6.
- Flash: 128 KiB.
- SRAM: 20 KiB.
- Clock: 8 MHz HSE, 72 MHz system clock, exact 48 MHz USB clock.
- Arduino core: `STMicroelectronics:stm32@3.0.0`, pinned in CI.
- Upload during development: SWD.
- Runtime bootloader: STM32 system-memory USART bootloader.
- USB stack: custom composite HID class using STM32Cube USB Device middleware
  bundled by STM32duino. Project code does not vendor or relicense that
  middleware.
- Matrix: 6 rows by 18 columns, four dynamic layers.
- Diodes: `COL2ROW`, cathode toward row.

The CI compile surrogate is:

```text
STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103CB,xserial=disabled,usb=HID,xusb=FS,opt=oslto,dbg=none,rtlib=nano,upload_method=swdMethod
```

The custom USB class owns USB registration. The application must not call the
stock STM32duino `Keyboard` or `Mouse` APIs.

## Board Pin Contract

| Function | Pins |
|---|---|
| Matrix rows | `PB10`, `PB11`, `PB12`, `PB13`, `PB14`, `PB15` |
| Matrix columns | `PA0`-`PA10`, `PA15`, `PB0`, `PB1`, `PB3`-`PB6` |
| USB D- / D+ | `PA11` / `PA12` |
| SWDIO / SWCLK | `PA13` / `PA14` |
| USB pull-up control | `PB7` |
| Caps Lock LED | `PB8` |
| HSE | `PD0` / `PD1` |

JTAG is disabled after reset so `PA15`, `PB3`, and `PB4` can serve the matrix;
SWD stays enabled. The board provides a correct 1.5 kOhm D+ pull-up through a
GPIO-controlled switch, plus accessible `BOOT0`, `NRST`, SWDIO, and SWCLK pads.

## Scope

### Included

- `COL2ROW` and `ROW2COL` matrix scanning.
- Configurable settle time; 30 microseconds by default.
- Global symmetric deferred debounce; 5 milliseconds by default.
- Stable press/release generation.
- Basic keyboard HID usages and physical modifiers.
- QMK modified-basic keycodes (`QK_MODS`).
- Transparent layer lookup.
- `MO`, `TG`, `TO`, and runtime-only `DF` layer actions.
- Boot-protocol 6KRO reports and host lock LEDs.
- Live VIA dynamic keymap integration.
- Dedicated VIA Raw HID interface.
- Dual-slot power-loss-safe internal flash storage.
- Minimal suspend/resume and remote wake behavior.
- VIA boot command and `QK_BOOT` coordination.
- STM32 system-memory USART bootloader jump and SWD recovery.

### Deferred To Phase 0.4 Or Later

- Mod-Tap, Layer-Tap, Tap Toggle, and tapping policies.
- Combos, one-shot keys, key overrides, and event hooks.
- Consumer, system, mouse, joystick, MIDI, and audio reports.
- NKRO.
- Macro execution and encoder execution.
- RGB, backlight effects, OLED, and pointing devices.
- Split keyboard transport.
- Deep sleep or STOP mode.
- RP2040 integration with the new keyboard engine.

## Architecture

```text
Arduino sketch
  |-- Protocol::task(now)
  |     `-- STM32 Raw HID transport
  |
  `-- Keyboard::task(now)
        |-- Matrix scan
        |-- deferred debounce
        |-- stable transition batch
        |-- layer/keycode resolver
        |-- 6KRO report builder
        `-- STM32 keyboard HID sink

STM32 adapters
  |-- GPIO/clock MatrixIO
  |-- one composite USB device
  |-- dual-slot flash Storage
  `-- boot coordinator
```

The behavior core does not include Arduino headers. Board adapters alone call
Arduino, STM32 HAL, or STM32Cube USB functions.

## Portable Interfaces

### Matrix I/O

```cpp
namespace via {

typedef uint32_t Pin;

enum DiodeDirection {
  kColToRow,
  kRowToCol,
};

class MatrixIO {
 public:
  virtual ~MatrixIO() {}
  virtual void inputPullup(Pin pin) = 0;
  virtual void driveLow(Pin pin) = 0;
  virtual void release(Pin pin) = 0;
  virtual bool read(Pin pin) = 0;
  virtual void delayMicroseconds(uint16_t us) = 0;
};

}  // namespace via
```

`release()` changes a selected output back to high-impedance input-pull-up. The
scanner never drives an unselected line high against a pressed switch.

`MatrixConfig` supplies dimensions, row and column pin arrays, diode direction,
settle time, debounce time, and caller-owned row buffers. Each row is a
`uint32_t`; columns above 32 are rejected.

### Keyboard HID

```cpp
namespace via {

struct KeyboardReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
};

class KeyboardHID {
 public:
  virtual ~KeyboardHID() {}
  virtual bool configured() const = 0;
  virtual bool send(const KeyboardReport& report) = 0;
  virtual bool sendComplete() = 0;
  virtual bool takeHostLeds(uint8_t& leds) = 0;
  virtual bool suspended() const = 0;
  virtual bool remoteWakeupAllowed() const = 0;
  virtual bool remoteWakeup() = 0;
};

}  // namespace via
```

`send(true)` means one exact report was accepted and must not be resent while in
flight. `sendComplete()` is queried only after acceptance. A busy endpoint
returns false without consuming the report.

### Keyboard Callbacks

The portable engine exposes only application effects:

```cpp
class KeyboardCallbacks {
 public:
  virtual ~KeyboardCallbacks() {}
  virtual void hostLedsChanged(uint8_t leds) { (void)leds; }
  virtual void bootloaderRequested() {}
};
```

The STM32 integration turns `bootloaderRequested()` into the boot coordinator
state machine. The portable engine never jumps to platform addresses itself.

## Matrix And Debounce

For `COL2ROW`:

1. Configure all columns as pull-up inputs.
2. Release every row.
3. Drive one row low.
4. Wait `settleUs`.
5. Read all columns; low means pressed.
6. Release that row before selecting the next.

`ROW2COL` swaps row and column roles while preserving active-low semantics.

Debounce follows global symmetric deferred behavior:

- A raw change copies the complete raw matrix into the candidate matrix and
  restarts one timer.
- Another raw change replaces the candidate and restarts the timer.
- Candidate state becomes stable only after `debounceMs` without any raw change.
- `debounceMs == 0` publishes immediately.
- Elapsed time uses unsigned subtraction and remains correct across
  `uint32_t` wraparound.

The VIA matrix-test callback returns stable debounced rows, never raw samples.

## Event Processing

Each stable transition is processed as one row-major batch:

1. Release every changed released position using its latched press-time code.
2. Apply every changed layer-action press.
3. Resolve and apply every remaining changed press against the resulting layer
   state.
4. Rebuild one complete desired keyboard report.

This ordering makes a simultaneous `MO(layer)` plus normal key deterministic.
Every physical position has one caller-owned `activeCodes` entry. VIA remapping
while a key is held cannot change its eventual release behavior.

## Layer Semantics

- Default layer state and transient layer state are separate.
- Lookup searches the highest active layer first.
- `KC_TRNS` continues to the next lower active layer.
- `KC_NO` stops lookup and performs no action.
- `MO(n)` increments a per-layer reference count on press and decrements the
  same latched action on release.
- `TG(n)` toggles transient layer `n` on press.
- `TO(n)` clears transient layer state, then activates only transient layer `n`;
  the default layer remains part of lookup.
- `DF(n)` changes the runtime default layer on press. It does not write flash;
  persistent default-layer behavior is deferred.
- Targets outside `Protocol::layers()` are no-ops.

## Supported Keycodes

| Range | Phase 0.3 behavior |
|---|---|
| `0x0000` | `KC_NO` |
| `0x0001` | `KC_TRNS` during lookup |
| `0x0004`-`0x00A4` | Keyboard/keypad HID usage |
| `0x00E0`-`0x00E7` | Physical modifiers |
| `0x0100`-`0x1FFF` | `QK_MODS` modified-basic keys |
| `0x5200`-`0x521F` | `TO(layer)` |
| `0x5220`-`0x523F` | `MO(layer)` |
| `0x5240`-`0x525F` | `DF(layer)` |
| `0x5260`-`0x527F` | `TG(layer)` |
| `0x7C00` | `QK_BOOT` |
| All other values | Safe no-op |

For `QK_MODS`, bits `8..11` encode modifiers and bit `12` selects the right
modifier side for the complete encoded modifier set. Unsupported values are
never truncated into an unrelated 8-bit HID usage.

## 6KRO Report State

Reports are rebuilt from all held `activeCodes` after each stable batch:

- Modifiers OR into the modifier byte and never consume a key slot.
- Duplicate HID usages appear once.
- Six or fewer distinct non-modifier usages are emitted in row-major order.
- More than six distinct usages emits `ErrorRollOver` (`0x01`) in all six slots.
- Dropping back to six or fewer rebuilds a normal report immediately.
- A desired report equal to the last accepted state is not sent.
- In-flight bytes are immutable. New state replaces only the next desired
  report.
- A release report remains pending until accepted.

## USB Composite Device

The custom STM32Cube class exposes two HID interfaces:

| Interface | Descriptor | Endpoint contract |
|---|---|---|
| 0 | Boot keyboard, subclass 1, protocol 1, no report ID | EP1 IN, 8-byte report; LED `SET_REPORT` via control endpoint |
| 1 | Vendor HID usage page `0xFF60`, usage `0x61`, no report ID | EP2 IN and EP2 OUT, fixed 32-byte reports |

The adapter implements both `KeyboardHID` and existing `via::Transport`. Each
interface has independent in-flight and receive state. Raw OUT is rearmed
immediately after copying exactly 32 bytes into static storage. USB callbacks
only copy fixed buffers and set flags; protocol, key processing, flash, and user
callbacks run from the main loop.

The descriptor snapshot test uses development VID/PID `0xCAFE/0x4003`. These
values are test-only. Hardware release requires an assigned product VID/PID and
a matching VIA V3 definition.

## Suspend And Host LEDs

- Keyboard LED output stores the latest Num/Caps/Scroll bitfield.
- The main loop drains it and calls `hostLedsChanged()` once per changed value.
- No keyboard report is sent while USB is suspended.
- Scanning and debouncing continue during suspend.
- First stable press requests remote wake once when the host allows it.
- Press and release entirely during suspend leaves no stale report.
- Resume marks the complete current report dirty and resends it.
- Phase `0.3` does not enter MCU STOP mode.

## Flash Storage

Physical layout:

```text
0x08000000-0x0801EFFF  application, 124 KiB
0x0801F000-0x0801F7FF  slot A, 2 KiB
0x0801F800-0x0801FFFF  slot B, 2 KiB
```

Each slot contains a 16-byte envelope followed by 2032 logical bytes. The
envelope contains magic, sequence, CRC32 of the complete logical area, and a
commit marker. The existing Protocol record, including its 12-byte header,
lives inside the logical area. Maximum Protocol payload is therefore 2020
bytes.

Transaction flow:

1. Select the inactive slot.
2. Erase its two 1 KiB flash pages.
3. Cache writes covering the Protocol header in 12 bytes of adapter RAM.
4. Program remaining logical bytes using halfword-safe one-to-zero writes.
5. On commit, program the latest cached Protocol header.
6. Calculate and program slot magic, next sequence, and logical-area CRC.
7. Program the commit marker last.

The active slot is never erased during replacement. Boot selects the newest
slot whose marker, envelope CRC, and Protocol record are valid. Sequence
comparison uses signed modular difference and is tested across wraparound.

`erase()` starts a provisional empty replacement and does not destroy the active
slot. No full flash-record RAM buffer or heap is used.

## Boot Coordination

Both the existing VIA boot callback and portable `QK_BOOT` request the same
STM32 coordinator.

1. Reject a new request while a boot transition is active.
2. Save dirty Protocol state; failure cancels the transition.
3. Stop accepting new key events.
4. Queue an all-release keyboard report.
5. Wait for accepted keyboard and VIA response transfers to complete when USB
   is configured and not suspended.
6. Disable the USB pull-up and peripheral.
7. Disable interrupts and reset clocks/peripherals required by the application.
8. Load MSP and reset vector from STM32F103 system memory at `0x1FFFF000`.
9. Jump to the ROM USART bootloader.

If USB is disconnected or suspended, report waiting is skipped. BOOT0 and SWD
remain unconditional recovery paths.

## Validation And Failure Behavior

- Zero dimensions, dimensions above supplied buffer capacities, more than 32
  columns, mismatched Protocol/matrix dimensions, null required buffers, and
  invalid pins cause `begin()` to fail before GPIO or USB side effects.
- Unsupported keycodes are no-ops.
- USB initialization failure prevents keyboard operation and leaves SWD/BOOT0
  recovery available.
- Invalid flash slots cause Protocol startup fallback to configured defaults;
  no partial state is published.
- Save failure leaves the old committed slot active and Protocol dirty.
- Endpoint busy never consumes or duplicates a report.
- Flash operations reject writes outside the two reserved slots.
- Runtime code does not call allocation APIs.

## Memory And Image Gates

Reference configuration: 6 rows, 18 columns, four layers, zero macros, zero
encoders, and zero custom state.

| Item | Expected bytes |
|---|---:|
| Dynamic keymap | 864 |
| Protocol load/reset buffer | 868 |
| Active press-time codes | 216 |
| Four matrix row arrays plus timer/state | <= 128 |
| HID and Raw HID fixed buffers | <= 256 |
| Portable engine objects/counters | <= 512 |

Hard CI gates:

- Application image: at most 112 KiB.
- `.data + .bss`: at most 12 KiB.
- Reserved stack margin: 4 KiB.
- Protocol required load buffer: at most 2020 bytes.
- No unresolved `malloc`, `calloc`, `realloc`, `free`, or C++ `new/delete`
  symbols from project runtime code.

## Testing

### Native Matrix Tests

- GPIO initialization and call order for both diode directions.
- Exactly one selected line; all other lines released.
- Settle delay occurs before reads.
- Correct raw bit mapping.
- Press and release bounce timelines.
- Candidate reset on noise.
- Exact debounce boundary, zero debounce, and timer wraparound.
- Stable matrix and changed-position masks.

### Native Keyboard Tests

- Release-before-press and layer-action-first batching.
- Highest-layer lookup and transparency.
- `MO`, duplicate `MO`, `TG`, `TO`, `DF`, and invalid targets.
- VIA remap while held releases the original code.
- Basic keys, all modifiers, and left/right `QK_MODS`.
- Duplicate usages and modifiers.
- Six keys, seven-key rollover, and recovery.
- Busy-send retry, accepted-send completion, coalescing, and pending release.
- Host LED coalescing.
- Suspend, denied/allowed remote wake, and resume resync.
- Unsupported codes remain no-ops.
- VIA matrix callback returns stable rows.

### Native USB And Flash Tests

- Exact USB descriptor bytes, interface count, endpoint uniqueness, usage page,
  usage, packet lengths, and absence of report IDs.
- Flash one-to-zero programming rules and address bounds.
- Newest valid slot selection and sequence wrap.
- Power cut after every erase, write, metadata, and marker operation.
- Every simulated reset selects either the complete old record or complete new
  record, never a partial record.

### CI

- Existing protocol and TinyUSB native tests remain unchanged and green.
- New native suites compile with C++11, warnings-as-errors, ASan, and UBSan.
- Arduino lint remains green.
- Existing Uno and RP2040 examples remain green.
- STM32duino `3.0.0` is pinned.
- Custom F103CB example and a second F103 portability target compile.
- Linker map enforces flash/RAM/storage partition gates.
- Descriptor snapshot and allocation-symbol checks pass.

## Hardware Certification

Hardware is not currently available. These gates block a hardware-verified
`0.3.0` release but do not block merging software-complete code marked
experimental:

- Genuine 128 KiB STM32F103CB identity confirmed.
- HSE and 48 MHz USB clock verified.
- D+ pull-up measured at 1.5 kOhm and controlled detach works.
- Logic analyzer confirms polarity, release behavior, and 30 microsecond settle.
- Full matrix scan cadence is at least 1 kHz.
- Bounce tests produce exactly one press and release.
- USB enumerates the boot keyboard and dedicated VIA interface on Windows,
  Linux, and macOS.
- Keyboard works in BIOS/UEFI.
- All modifiers, six keys, rollover, recovery, and host LEDs pass.
- VIA V3 discovery, matrix test, remap, persistence, and factory reset pass.
- Suspend/resume/remote wake produce no stuck key.
- VIA and `QK_BOOT` release/response transfers finish before detach and ROM jump.
- Power interruption at every flash phase preserves old or new complete state.
- SWD and BOOT0 recovery pass.
- Measured image and RAM stay inside gates.

## Milestones

1. Portable matrix scanner and debounce.
2. Portable event, layer, keycode, and 6KRO engine.
3. STM32F103 Cube USB composite and descriptor tests.
4. STM32F103 dual-slot flash storage and fault simulation.
5. Board integration, ROM boot coordinator, reference example, CI, and docs.
6. Hardware certification when the custom PCB is available.

Each milestone is independently tested, reviewed, committed, and pushed on
`feat/0.3-stm32f103-keyboard-mvp`.

## References

- [STM32duino 3.0.0](https://github.com/stm32duino/Arduino_Core_STM32/releases/tag/3.0.0)
- [STM32duino installation](https://github.com/stm32duino/Arduino_Core_STM32/wiki/Getting-Started)
- [STM32F103 datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32 system-memory boot mode](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)
- [QMK matrix behavior](https://docs.qmk.fm/how_a_matrix_works)
- [QMK debounce behavior](https://docs.qmk.fm/feature_debounce_type)
- [QMK basic keycodes](https://docs.qmk.fm/keycodes_basic)
- [QMK keymap and transparency](https://docs.qmk.fm/keymap)
- [QMK layers](https://docs.qmk.fm/feature_layers)
- [QMK Raw HID](https://docs.qmk.fm/features/rawhid)
- [QMK host LEDs](https://docs.qmk.fm/features/led_indicators)
- [QMK keycode ABI](https://github.com/qmk/qmk_firmware/blob/master/quantum/keycodes.h)
