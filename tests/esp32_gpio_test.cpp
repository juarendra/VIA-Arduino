#include <cassert>
#include <iostream>

#include "VIA_ESP32_GPIO.h"
#include "fakes/Arduino.h"

void test_input_pullup() {
  via::esp32::MatrixIOArduino io;
  io.inputPullup(5);
  assert(FakeArduino::mode[5] == INPUT_PULLUP);
}

void test_drive_low_and_release() {
  via::esp32::MatrixIOArduino io;
  io.driveLow(7);
  assert(FakeArduino::mode[7] == OUTPUT);
  assert(FakeArduino::value[7] == LOW);

  io.release(7);
  assert(FakeArduino::mode[7] == INPUT_PULLUP);
}

void test_active_low_read() {
  via::esp32::MatrixIOArduino io;
  io.driveLow(3);
  // Driven low: key pressed (active-low).
  assert(io.read(3));
  // Released high: no key.
  FakeArduino::value[3] = HIGH;
  assert(!io.read(3));
}

void test_delay_microseconds_returns() {
  via::esp32::MatrixIOArduino io;
  // The fake is a no-op; this call must simply return (guards against an
  // unqualified call resolving to the member and recursing forever).
  io.delayMicroseconds(100);
}

int main() {
  test_input_pullup();
  test_drive_low_and_release();
  test_active_low_read();
  test_delay_microseconds_returns();
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
