# VIA-Arduino

The ultimate Arduino implementation of the QMK VIA Custom Keyboard Protocol.

## 🚀 Key Features & Upgrades
- **32-Column Matrix Support**: Rewritten matrix packer dynamically supports up to 32 columns, fixing the 8-column limitation that broke standard keyboards in VIA's Key Tester.
- **Bootloader Jump (0x0B)**: Full support for jumping to the bootloader directly from the VIA Configurator UI.
- Dynamic keymaps, EEPROM storage, Macros, and RGBLight support.

## 📖 Usage Manual

```cpp
#include <VIA_Protocol.h>

class MyKeyboardCallbacks : public via::Callbacks {
public:
  uint32_t matrixRow(uint8_t row) const override {
    // Return up to 32 bits representing the column states for this row
    return readMatrixRow(row); 
  }
  
  void bootloaderJump() override {
    // Execute platform-specific bootloader jump
    reset_usb_boot(0, 0); 
  }
};
```

## 🛠 Installation
1. Download this repository as a `.zip` file.
2. In the Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library...**
3. Select the downloaded `.zip` file.

## 📄 License
MIT License.
