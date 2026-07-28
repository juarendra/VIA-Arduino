# VIA-Arduino - The Universal QMK VIA Protocol

****Connect any custom Arduino keyboard to the VIA Configurator.****

Drive **32-column matrices** effortlessly • **Bootloader Jump Support** • **Macro & Layer Storage** • [**Built for Custom Keyboards**](https://github.com/juarendra/VIA-Arduino)

[![Build Status](https://github.com/juarendra/VIA-Arduino/actions/workflows/build.yml/badge.svg)](https://github.com/juarendra/VIA-Arduino/actions/workflows/build.yml) [![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE) [![Community](https://img.shields.io/badge/github-juarendra-orange.svg?logo=github)](https://github.com/juarendra)

## ⚡ Get Configured in 30 Seconds

```cpp
#include <VIA_Protocol.h>

class MyKeyboard : public via::Callbacks {
public:
  uint32_t matrixRow(uint8_t row) const override {
    return readMatrixRow(row); // Supports up to 32 cols!
  }
  void bootloaderJump() override {
    reset_usb_boot(0, 0); 
  }
};
```

**✅ Works on ESP32, RP2040, 32u4, Teensy, and 50+ other platforms**

## Table of Contents
- [⚡ Quick Start](#-get-configured-in-30-seconds)
- [🚀 Why This Library?](#-why-this-library)
- [📦 Installation](#-installation)
- [📄 License](#-license)

## 🚀 Why This Library?

| **32-Column Matrix** | **Bootloader Jump** | **Fully Dynamic** | **Universal** |
|---|---|---|---|
| QMK-standard dynamic packing | Command 0x0B implemented | EEPROM macros and layers | Works on 50+ platforms |

**🎯 Performance**: Dynamic byte packing based on column count • Minimal RAM footprint.
**🔧 Developer Experience**: Object-oriented callbacks • Instantly recognized by VIA.

## 📦 Installation
1. Download this repository as a `.zip` file.
2. In the Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library...**
3. Select the downloaded `.zip` file.
4. (Optional) Check the `examples/` directory for full usage.

## 📄 License
This project is licensed under the MIT License - see the LICENSE file for details.
