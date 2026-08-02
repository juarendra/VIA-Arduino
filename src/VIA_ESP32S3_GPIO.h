#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

#include "VIA_Matrix.h"
#include <Arduino.h>

namespace via {
namespace esp32s3 {

class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) != HIGH; }
  void delayMicroseconds(uint16_t us) override { delayMicroseconds(us); }
};

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3
