#pragma once
#if defined(ARDUINO_ARCH_NRF52) && defined(NRF52840_XXAA)
#include <Arduino.h>
#include "VIA_Matrix.h"
namespace via { namespace nrf52 {
class MatrixIOArduino : public via::MatrixIO {
 public:
  void inputPullup(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  void driveLow(Pin pin) override { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(Pin pin) override { pinMode(pin, INPUT_PULLUP); }
  bool read(Pin pin) override { return digitalRead(pin) == LOW; }
  void delayMicroseconds(uint16_t us) override { ::delayMicroseconds(us); }
};
}}
#endif
