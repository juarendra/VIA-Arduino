#include "VIA_TinyUSB_Keyboard.h"

#if __has_include(<Adafruit_TinyUSB.h>)

namespace via {
namespace tinyusb {
namespace {

uint8_t const kKeyboardDescriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(),
};

}  // namespace

Keyboard::Keyboard()
    : hid_(kKeyboardDescriptor, sizeof(kKeyboardDescriptor),
           HID_ITF_PROTOCOL_KEYBOARD, 2, false) {}

bool Keyboard::begin(const char* interfaceName) {
  if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
  hid_.setPollInterval(2);
  hid_.setStringDescriptor(interfaceName);
  return hid_.begin();
}

bool Keyboard::report(uint8_t modifiers, const uint8_t keys[6]) {
  return hid_.ready() && hid_.keyboardReport(0, modifiers,
                                               const_cast<uint8_t*>(keys));
}

bool Keyboard::release() {
  uint8_t empty[6] = {};
  return report(0, empty);
}

bool Keyboard::ready() { return hid_.ready(); }

}  // namespace tinyusb
}  // namespace via

#endif
