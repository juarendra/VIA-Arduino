#include <assert.h>
#include "VIA_nRF52_GPIO.h"
#include "VIA_nRF52_BLE.h"

int main() {
  via::nrf52::MatrixIOArduino io;
  io.inputPullup(D0);
  assert(FakeArduino::mode[D0] == INPUT_PULLUP);
  io.driveLow(D1);
  assert(FakeArduino::mode[D1] == OUTPUT && FakeArduino::value[D1] == LOW);
  io.release(D1);
  assert(FakeArduino::mode[D1] == INPUT_PULLUP);

  BLEHidAdafruit service;
  via::nrf52::BLEKeyboardHID hid(service);
  assert(hid.begin());
  Bluefruit.connectedResult = true;
  via::KeyboardReport report = {0x02, 0, {0x04, 0x05, 0, 0, 0, 0}};
  assert(hid.send(report));
  assert(service.lastModifier == 0x02);
  assert(service.lastKeys[0] == 0x04 && service.lastKeys[1] == 0x05);
  service.dispatchKeyboardLeds(0x03);
  uint8_t leds = 0;
  assert(hid.takeHostLeds(leds) && leds == 0x03);
  assert(!hid.takeHostLeds(leds));
}
