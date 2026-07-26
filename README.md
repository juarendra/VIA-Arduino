# VIA_Arduino

[![CI](https://github.com/juarendra/VIA-Arduino/actions/workflows/ci.yml/badge.svg)](https://github.com/juarendra/VIA-Arduino/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Arduino Library](https://img.shields.io/badge/Arduino-library-00979D.svg)](https://docs.arduino.cc/arduino-cli/library-specification/)

Portable Arduino core for keyboards and macropads compatible with **VIA V3**.
It separates the VIA protocol from USB and storage so one firmware design can
target RP2040, ESP32-S2/S3, SAMD, STM32, or another board with native USB.

## What version 0.1.0 provides

- VIA protocol v13: dynamic keymap, dynamic keymap buffer, layer count, macros,
  EEPROM commit/reset, keyboard values, and QMK-style custom values.
- Configurable rows, columns, layers, default keymap, macro count, and storage.
- A `RGBLight` custom-value adapter for VIA `qmk_rgblight` channel 2.
- Delayed persistence through a `Storage` adapter, with CRC-protected records.
- A portable `Transport` interface for the dedicated 32-byte Raw HID endpoint.
- Native C++ tests and an Arduino IDE example that compiles without a USB
  library dependency.

This package is the reusable core extracted from the proven OSUpad Clone VIA
firmware. It is **not** a replacement for an entire USB stack or QMK.

## USB requirement

VIA needs a dedicated vendor HID interface with usage page `0xFF60`, usage
`0x61`, no report ID, and exactly 32-byte input/output reports. It must be
separate from keyboard, mouse, or consumer HID interfaces. A USB-to-UART board
such as CH340 cannot provide this by itself.

The library owns protocol handling after the packet reaches `Transport`; the
Arduino core or a board adapter owns USB descriptors and endpoints.

| Target | Core status | USB adapter status |
| --- | --- | --- |
| RP2040 with TinyUSB | Supported by protocol core | First reference adapter planned |
| ESP32-S2/S3 TinyUSB | Supported by protocol core | First reference adapter planned |
| SAMD / nRF52 TinyUSB | Supported by protocol core | Adapter required |
| STM32 native USB | Supported by protocol core | Adapter required |
| STM32F1 + USBComposite | Supported by protocol core | OSUpad-specific adapter remains in its firmware |
| USB-to-UART only boards | Not supported | No native USB device endpoint |

See [Porting a USB adapter](docs/PORTING.md) for the exact contract.

## Install

### Arduino IDE ZIP install

1. Download this repository as a ZIP file.
2. In Arduino IDE, choose **Sketch → Include Library → Add .ZIP Library**.
3. Open **File → Examples → VIA_Arduino → Protocol_Self_Test**.

### Manual install

Clone or copy this repository to the Arduino sketchbook `libraries` directory
as `VIA-Arduino`. The public include is:

```cpp
#include <VIA_Arduino.h>
```

## Minimal protocol setup

```cpp
#include <VIA_Arduino.h>

uint16_t keymap[4] = {0x0004, 0x0005, 0x0014, 0x001A};
const uint16_t defaults[4] = {0x0004, 0x0005, 0x0014, 0x001A};
uint8_t macros[64];

MyRawHidTransport rawHid;   // Board-specific native USB adapter
MyPersistentStorage storage; // EEPROM, NVS, or reserved flash adapter

via::Config config = {
  1, 2, 2, keymap, defaults, macros, sizeof(macros), 2, 1, 750,
};
via::Protocol keyboard(config, rawHid, &storage);

void setup() {
  rawHid.begin();
  keyboard.begin(millis());
}

void loop() {
  keyboard.task(millis());
  // Scan physical keys and emit the keycodes from keyboard.keycode(...).
}
```

`MyRawHidTransport` implements `receive()` and `send()` for a Raw HID USB
endpoint. `MyPersistentStorage` implements `read()`, `write()`, `commit()`, and
`erase()` for the target board. Neither adapter is hidden: this prevents a
library update from silently changing board flash layout or USB descriptors.

## RGB custom value

`via::RGBLight` implements the VIA `qmk_rgblight` custom-value channel. Supply
an `RGBLightCallbacks` object that renders `RGBLightState` to your LED driver.
The state joins keymap and macro data in the saved settings record.

## Scope and roadmap

Version 0.1.0 intentionally delivers the portable VIA core first. It does not
scan switches, send keyboard HID reports, render LEDs, or install USB
descriptors automatically—those are board-level responsibilities.

Planned next milestones:

1. RP2040 TinyUSB Raw HID + keyboard composite reference adapter.
2. ESP32-S2/S3 TinyUSB adapter and Preferences/NVS storage adapter.
3. SAMD and STM32 adapters, dual-page flash persistence examples, mouse and
   consumer report helpers.
4. Integration examples that port OSUpad behavior without copying its firmware.

## Documentation

- [Porting a USB and storage adapter](docs/PORTING.md)
- [Arduino Library Manager readiness](docs/ARDUINO_LIBRARY_MANAGER.md)
- [Protocol self-test](examples/Protocol_Self_Test/Protocol_Self_Test.ino)
- [Changelog](CHANGELOG.md)

## License

MIT. See [LICENSE](LICENSE).
