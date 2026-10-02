#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Matrix.h"
#include <Arduino.h>

namespace via {
namespace esp32 {

/* Generic ESP32 matrix IO (WROOM-32, WROOM-32E, S3). Active-low, pull-up
 * inputs, matching the Arduino digitalWrite/digitalRead contract. */
class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) != HIGH; }
  void delayMicroseconds(uint16_t us) override { ::delayMicroseconds(us); }
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
