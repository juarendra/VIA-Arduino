# VIA-Arduino - The Ultimate Keyboard Protocol Engine

****Instantly connect any custom Arduino keyboard to the VIA Configurator ecosystem.****

Drive **massive 32-column matrices** effortlessly • **Native Bootloader Jump Support** • **Dynamic EEPROM Mapping** • [**Built for Custom Keyboards**](https://github.com/juarendra/VIA-Arduino)

[![Platform](https://img.shields.io/badge/Platform-Arduino-blue.svg)](https://www.arduino.cc/) [![Version](https://img.shields.io/badge/Version-1.0.0-brightgreen.svg)]() [![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE) [![Community](https://img.shields.io/badge/github-juarendra-orange.svg?logo=github)](https://github.com/juarendra) [![Library Size](https://img.shields.io/badge/Size-Ultra%20Light-brightgreen.svg)]()

## ⚡ Get Configured in 30 Seconds

```cpp
#include <VIA_Protocol.h>

class MyKeyboard : public via::Callbacks {
public:
  uint32_t matrixRow(uint8_t row) const override {
    // Dynamic 32-column support automatically packed for VIA
    return readMatrixRow(row); 
  }
  void bootloaderJump() override {
    // 0x0B remote reset support
    reset_usb_boot(0, 0); 
  }
};
```

**✅ Fully compatible with Arduino, ESP32, RP2040, ATmega32u4, Teensy, and 50+ other embedded platforms**

## Table of Contents
- [⚡ Quick Start](#-get-sensing-in-30-seconds)
- [🚀 Why This Library?](#-why-this-library)
- [📚 Core API Reference](#-core-api-reference)
- [🌍 Platform Compatibility](#-platform-compatibility)
- [📦 Installation](#-installation)
- [📄 License](#-license)

## 🚀 Why This Library?

| **32-Column Matrix** | **Bootloader Jump** | **Fully Dynamic** | **Universal** |
|---|---|---|---|
| QMK-standard dynamic packing | Command 0x0B implemented | EEPROM macros and layers | Works on 50+ platforms |

**🎯 Performance**: Intelligent dynamic byte packing scales memory overhead exactly to your column count.
**🔧 Developer Experience**: Pure Object-Oriented callbacks • Automatically detected by VIA web interface.

## 💡 Advanced Usage Example

```cpp
// Advanced Usage: Deep VIA Integration with 32-Column Support
#include <VIA_Protocol.h>

class MyCustomKeyboard : public via::Callbacks {
public:
  // Dynamically map a custom split keyboard matrix
  uint32_t matrixRow(uint8_t row) const override {
    uint32_t state = 0;
    // Iterate through up to 32 columns dynamically!
    for(int col = 0; col < 12; col++) {
      if(isKeyPressed(row, col)) {
        state |= (1UL << col); // Shift bits dynamically based on column count
      }
    }
    return state; 
  }
  
  // Respond to VIA Configurator's remote flash request (Command 0x0B)
  void bootloaderJump() override {
    Serial.println("VIA Command 0x0B received. Entering Bootloader...");
    // Platform specific jump code goes here (e.g., Pico or Teensy)
    reset_usb_boot(0, 0); 
  }
};

MyCustomKeyboard keyboard;

void setup() {
  // Initialize USB HID endpoints...
}

void loop() {
  // Feed incoming raw USB HID packets into the protocol engine
  // keyboard.handleData(rx_buffer, 32);
}
```

## 📚 Core API Reference

- `virtual uint32_t matrixRow(uint8_t row)`: Override this to return the bitmask of pressed keys for a given row. Now supports up to 32 bits (columns).
- `virtual void bootloaderJump()`: Override this to trigger a hardware reset (DFU/Bootloader mode) directly from the VIA GUI.
- `void handleData(uint8_t* data, uint8_t length)`: Feed USB HID payloads into this function to let the protocol handle mapping, layout changes, and macros.
- `void loadEEPROM()` / `void saveEEPROM()`: Built-in wrappers for layout persistence.

## 🌍 Platform Compatibility

This library is engineered to be platform-agnostic. Below is the verified compatibility matrix:

### 🟩 ESP32 Family (Espressif)
- **ESP32 Classic** (WROOM/WROVER)
- **ESP32-S2 / S3**
- **ESP32-C3 / C6**

### 🟦 Arduino Core & AVR
- **Arduino Uno R3 / R4 Minima & WiFi**
- **Arduino Mega 2560**
- **Arduino Nano / Every / 33 IoT**
- **ATtiny85 / ATmega32u4 (Leonardo/Pro Micro)**

### 🟪 ARM & Advanced Cortex
- **Teensy 4.0 / 4.1 / 3.2 / LC**
- **Raspberry Pi Pico (RP2040 / RP2350)**
- **STM32 (Bluepill / Blackpill)**

## 📦 Installation
1. Download this repository as a `.zip` file.
2. In the Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library...**
3. Select the downloaded `.zip` file.
4. *(Optional) Check the `examples/` directory for full usage implementation.*

## 📄 License
This project is licensed under the MIT License - see the LICENSE file for details.
