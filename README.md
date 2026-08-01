# VIA-Arduino

[![Version](https://img.shields.io/badge/version-0.2.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

VIA-Arduino 0.2.0 is a portable C++11 core for VIA protocol v13. It processes
fixed 32-byte Raw HID packets and manages dynamic keymaps, macros, layout
options, encoder maps, custom values, and optional persistence.

This release is not complete keyboard firmware. Applications still own matrix
scanning, layer selection, QMK keycode execution, and keyboard/mouse/consumer
HID reports. VIA discovery also requires a compatible native-USB Raw HID
interface and a matching VIA V3 definition; it is not automatic.

## Tested Targets

CI verifies only these configurations:

- Native C++11 protocol tests with warnings treated as errors.
- Arduino Uno compilation of the transport-independent self-test. Uno has no
  built-in VIA Raw HID adapter in this library.
- Raspberry Pi Pico (RP2040) compilation with the Earle Philhower core and
  Adafruit TinyUSB, including Raw HID, boot-keyboard, and EEPROM adapters.

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
void setup() {
  protocol.begin(millis());
}

void loop() {
  protocol.task(millis());
}
```

See [`Protocol_Self_Test`](examples/Protocol_Self_Test/Protocol_Self_Test.ino)
for the portable core and
[`RP2040_VIA_RawHID`](examples/RP2040_VIA_RawHID/RP2040_VIA_RawHID.ino) for
the compile-tested RP2040 adapters.

## Persistence

`via::Storage` is optional. When supplied, `Config::loadBuffer` is mandatory:
it must be caller-owned and at least `requiredLoadBufferSize()` bytes. The core
rejects overlap with configured keymap, encoder-map, and macro buffers before
accessing storage. It cannot inspect state owned by a `CustomValue` handler, so
the caller must keep that active state outside the load workspace as required
by the [porting contract](docs/PORTING.md).

```cpp
uint8_t macros[64];
uint8_t storageBytes[128];
uint8_t loadBuffer[sizeof(keymap) + sizeof(macros) + sizeof(uint32_t)];
via::MemoryStorage storage(storageBytes, sizeof(storageBytes));

via::Config storedConfig = {
    1, 2, 1, keymap, defaults, macros, sizeof(macros), 0, 1, 750,
    0, 0, nullptr, nullptr, loadBuffer, sizeof(loadBuffer),
};
via::Protocol storedProtocol(storedConfig, transport, &storage);

bool loaded = storedProtocol.load();
bool saved = storedProtocol.save();
```

`MemoryStorage` is for tests and examples and does not survive power loss.
Board storage adapters must reserve enough space for the 12-byte record header
plus the complete payload. See [`docs/PORTING.md`](docs/PORTING.md).

The 0.2.0 storage schema is version 2. Version 1 records from 0.1.0 are
rejected; `begin()` performs its normal one-time startup fallback to configured
built-in defaults. No automatic record migration or save occurs, so users must
reconfigure and save new state.

## Configuration And Callbacks

The optional `Config` fields appended in 0.2.0 are:

- `defaultLayoutOptions`
- `encoderCount`, `encoderMap`, and `defaultEncoderMap`; each encoder stores two
  16-bit keycodes per layer, counterclockwise then clockwise
- `loadBuffer` and `loadBufferBytes`
- `matrixStateEnabled`, `eepromResetEnabled`, and `bootloaderEnabled`

All three security flags default to `false`. Matrix-state requests return
zeros, while EEPROM reset and bootloader jump return `0xFF`, until their
individual flags are enabled.

Derive from `via::Callbacks` only for application hooks you need:

- `matrixRow(uint8_t) -> uint32_t`: matrix-test state for up to 32 columns
- `deviceIndication(uint8_t)`: full indication value from command `0x03/0x05`
- `layoutOptionsChanged(uint32_t)`: new layout-option bitfield
- `changed()`: mutable protocol state changed
- `bootloaderJump()`: runs only from `task()`, after an enabled bootloader
  command response is sent successfully

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

## License

Released under the [MIT License](LICENSE).
