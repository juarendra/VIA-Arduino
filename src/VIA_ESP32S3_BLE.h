#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

#include "VIA_Keyboard.h"
#include <BleKeyboard.h>

namespace via {
namespace esp32s3 {

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

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3
