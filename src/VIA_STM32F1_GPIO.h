#pragma once

#if defined(ARDUINO_ARCH_STM32) || defined(STM32F1xx)

#include "VIA_Matrix.h"

#include <Arduino.h>

namespace via {
namespace stm32f1 {

class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) != LOW; }
  void delayMicroseconds(uint16_t us) override { delayMicroseconds(us); }
};

}  // namespace stm32f1
}  // namespace via

#endif  // ARDUINO_ARCH_STM32 || STM32F1xx
