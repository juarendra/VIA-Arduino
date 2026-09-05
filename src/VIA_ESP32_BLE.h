#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Keyboard.h"
#include <BleKeyboard.h>

namespace via {
namespace esp32 {

/* BleKeyboard (NimBLE HID) report adapter for generic ESP32 boards,
 * including ESP32-WROOM-32 which has no native USB. */
class BleKeyboardHID : public via::KeyboardHID {
 public:
  explicit BleKeyboardHID(BleKeyboard& ble) : ble_(ble) {}

  bool configured() const override { return ble_.isConnected(); }

  bool send(const via::KeyboardReport& r) override {
    if (!ble_.isConnected()) return false;
    ble_.sendReport(reinterpret_cast<const KeyReport*>(&r));
    return true;
  }

  bool sendComplete() override { return true; }

  bool takeHostLeds(uint8_t& leds) override {
    // BLE HID does not have host LED output report
    (void)leds;
    return false;
  }

  bool suspended() const override { return !ble_.isConnected(); }
  bool remoteWakeupAllowed() const override { return false; }
  bool remoteWakeup() override { return false; }

 private:
  BleKeyboard& ble_;
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
