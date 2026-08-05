# VIA-Arduino

[![Version](https://img.shields.io/badge/version-0.5.0--experimental-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

VIA-Arduino is a portable C++11 firmware library for Arduino-compatible
microcontrollers that implements the VIA protocol v13, matrix scanning,
debounce, layer/keycode processing, 6KRO keyboard reports, and platform
adapters for RP2040, STM32F103, and ESP32-S3.

## Tested Targets

CI verifies these configurations on every push:

- Native C++11 protocol, matrix, keyboard, encoder, battery, sleep, flash,
  descriptor, and TinyUSB tests with `-Wall -Wextra -Werror`.
- Arduino Uno compilation of the transport-independent self-test.
- Raspberry Pi Pico (RP2040) compilation with Adafruit TinyUSB (Raw HID,
  boot-keyboard, EEPROM).
- STM32F103 (compile-only, Cube USB adapters, hardware not yet tested).
- ESP32-S3 (compile-only, dual-mode USB+BLA adapter, hardware not yet tested).
- nRF52840 (nice!nano v2) compilation with Adafruit Bluefruit (AirVIA BLE transport). Hardware not yet tested.

Other architectures may work through the portable interfaces, but are not
verified by this project.

## Quick Start

Use `via::Protocol::process()` when an application already owns packet I/O:

```cpp
#include <VIA_Arduino.h>

uint16_t keymap[2] = {0x0004, 0x0005};
const uint16_t defaults[2] = {0x0004, 0x0005};
via::MemoryTransport transport;
via::Config config = {1, 2, 1, keymap, defaults};
via::Protocol protocol(config, transport);

void handleViaPacket(uint8_t packet[via::kPacketSize]) {
  protocol.process(packet, millis());
}
```

Use `task()` with a `via::Transport`; it receives one packet, processes it,
retries failed response sends, and handles timed saves:

```cpp
void setup() { protocol.begin(millis()); }
void loop()  { protocol.task(millis()); }
```

## Modules

### Protocol Core (VIA v13)

All VIA protocol v13 commands including dynamic keymaps, macros (storage-only),
layout options, encoder maps, custom values, factory reset, and bootloader
jump with optional persistence. See [Supported Commands](#supported-commands).

### Matrix Scanner

`COL2ROW` and `ROW2COL` matrix scanning with configurable settle time (default
30 us) and global symmetric deferred debounce (default 5 ms). 32 columns
maximum per row, zero heap.

```cpp
#include <VIA_Matrix.h>

via::Matrix matrix(matrixConfig, matrixIO);
matrix.begin();
matrix.task(millis());
if (matrix.hasChanged()) { /* ... */ }
```

### Keyboard Engine

Stable press/release event batching, transparent layer lookup, basic/modifier/
QK_MODS keycodes, and boot-protocol 6KRO reports with busy-endpoint retry,
host LED feedback, suspend/resume, and remote wake.

```cpp
#include <VIA_Keyboard.h>

via::Keyboard keyboard({rows, cols}, matrix, protocol, hid, activeCodes);
keyboard.begin();
keyboard.task(millis());
```

### Keycode Classification

Header-only QMK 0.0.8 keycode range decoder supporting `KC_NO`, `KC_TRNS`,
basic HID usages, physical modifiers, `QK_MODS`, `MO`, `TG`, `TO`, `DF`,
`QK_BOOT`, and safe no-op for everything else.

### Peripherals

- `VIA_Encoder` — gray-code quadrature state machine, configurable debounce.
- `VIA_Battery` — ADC voltage-to-percentage conversion with moving average.
- `VIA_SleepMgr` — idle timeout state machine feeding platform deep sleep.

### Platform Adapters

| Adapter | Platform | Feature |
|---|---|---|
| `VIA_TinyUSB_RawHID` | RP2040, ESP32-S3 | VIA transport (usage page 0xFF60) |
| `VIA_TinyUSB_Keyboard` | RP2040, ESP32-S3 | USB HID boot keyboard |
| `VIA_EEPROMStorage` | RP2040, AVR | EEPROM persistence |
| `VIA_STM32F1_USB` | STM32F103 | Cube USB composite (keyboard + VIA) |
| `VIA_STM32F1_Flash` | STM32F103 | Dual-slot atomic flash storage |
| `VIA_STM32F1_Boot` | STM32F103 | ROM USART bootloader coordinator |
| `VIA_STM32F1_GPIO` | STM32F103 | Arduino GPIO matrix IO |
| `VIA_ESP32S3_GPIO` | ESP32-S3 | Arduino GPIO matrix IO |
| `VIA_ESP32S3_NVS` | ESP32-S3 | Preferences NVS persistence |
| `VIA_ESP32S3_BLE` | ESP32-S3 | NimBLE BLE HID keyboard adapter |

### Examples

- [`Protocol_Self_Test`](examples/Protocol_Self_Test) — portable protocol core
- [`RP2040_VIA_RawHID`](examples/RP2040_VIA_RawHID) — wired keyboard with VIA
- [`STM32F103_Keyboard_MVP`](examples/STM32F103_Keyboard_MVP) — compile-only
  wired keyboard reference
- [`ESP32S3_VIA_BLE`](examples/ESP32S3_VIA_BLE) — compile-only wireless
  keyboard with USB VIA config and BLE typing

## Persistence

`via::Storage` is optional. When supplied, `Config::loadBuffer` is mandatory:
it must be caller-owned and at least `requiredLoadBufferSize()` bytes. The core
rejects overlap with mutable or default keymap and encoder-map buffers, and with
the macro buffer, before accessing storage. The same workspace stages factory
defaults before a reset is made durable. The core cannot inspect state owned by
a `CustomValue` handler, so the caller must keep that active state outside the
workspace as required by the [porting contract](docs/PORTING.md).

Before publishing loaded custom state, or erasing storage for a factory reset,
the core calls `CustomValue::validateState()` on the staged bytes. Its default
accepts exactly `stateSize()` bytes. A handler that can reject persisted state
must override this method without mutating active state; `false` blocks the
operation, while `true` guarantees that `loadState()` for the same bytes
succeeds and publishes them. `loadState()` must remain non-mutating when it
returns `false`.

`load()` reads the complete payload once into this workspace, then checks CRC
and custom state against those exact bytes before publishing them. A successful
load publishes callbacks and then clears dirty state; a failed load preserves
the prior dirty flag and active state.

The 0.2.0 storage schema is version 2. Version 1 records from 0.1.0 are
rejected; `begin()` performs its normal one-time startup fallback to configured
built-in defaults.

## Configuration And Callbacks

The optional `Config` fields appended in 0.2.0 are:

- `defaultLayoutOptions`
- `encoderCount`, `encoderMap`, and `defaultEncoderMap`
- `loadBuffer` and `loadBufferBytes`
- `matrixStateEnabled`, `eepromResetEnabled`, and `bootloaderEnabled`

All three security flags default to `false`. Matrix-state requests return
zeros, while EEPROM reset and bootloader jump return `0xFF`, until their
individual flags are enabled.

Derive from `via::Callbacks` only for application hooks you need:

- `matrixRow(uint8_t) -> uint32_t`: matrix-test state for up to 32 columns
- `deviceIndication(uint8_t)`: full indication value from command `0x03/0x05`
- `layoutOptionsChanged(uint32_t)`: final layout-option bitfield after a command,
  successful stored/default startup load, direct load, or factory reset
- `changed()`: mutable protocol state changed
- `bootloaderJump()`: runs only from `task()`, after an enabled bootloader
  command response transfer completes

## Supported Commands

| Command | Support |
|---|---|
| `0x01` | Protocol version (`0x000D`) |
| `0x02` | Get uptime, layout options, optional matrix state, firmware version, or QMK keycodes ABI (`0.0.8`) |
| `0x03` | Set layout options or device indication |
| `0x04`-`0x06` | Get/set keycode; reset keymap and encoder maps |
| `0x07`-`0x09` | Custom-value set/get/save through `via::CustomValue` |
| `0x0A` | Factory-reset persistent state, opt-in |
| `0x0B` | Bootloader jump, opt-in |
| `0x0C`-`0x10` | Macro count, size, buffer get/set, and reset |
| `0x11`-`0x13` | Layer count and dynamic-keymap buffer get/set |
| `0x14`-`0x15` | Encoder keycode get/set |

Unsupported commands and invalid gated operations return `0xFF` in byte 0.

## Installation

Install `VIA_Arduino` from Arduino Library Manager, or install a release ZIP
through **Sketch > Include Library > Add .ZIP Library...**.

ESP32-S3 examples additionally need NimBLE-Arduino and ESP32-BLE-Keyboard
from the Arduino Library Manager.

## nice!nano v2 Installation (Experimental)

Requires **Adafruit nRF52 BSP 1.7.0** (pin to exactly this version).
Select board: **Pro Micro nRF52840**.

To flash:
1. Double-tap the reset button quickly to enter the UF2 bootloader.
2. The board will appear as a USB drive (e.g., `NICENANO`).
3. Compile and upload from Arduino IDE.

**Wiring Table Example:**
```text
Rows: D0, D1
Columns: D2, D3, D4
Each switch connects one row to one column; active-low with pull-ups.
```

### AirVIA BLE Workflow

1. Pair the keyboard via OS Bluetooth settings.
2. Open a Chromium-based browser (Chrome/Edge) and navigate to VIA.
3. Use the AirVIA connection method (Web Bluetooth).
4. Load the `nice_nano_v2_VIA_BLE.json` design file if prompted.
5. Sync, remap keys, and verify changes persist after a reset or power cycle.
6. **Recovery:** If connection fails, remove the BLE bond from your OS and explicitly erase VIA storage by clearing the internal file system.

## License

Released under the [MIT License](LICENSE).
