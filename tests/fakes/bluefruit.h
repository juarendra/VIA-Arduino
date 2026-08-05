#pragma once
#include <stdint.h>
#include <functional>

struct BLEHidAdafruit {
  uint8_t lastModifier = 0;
  uint8_t lastKeys[6] = {0};
  std::function<void(uint16_t, uint8_t)> ledCallback;

  bool keyboardReport(uint8_t modifier, const uint8_t keycode[6]) {
    lastModifier = modifier;
    for(int i=0; i<6; i++) lastKeys[i] = keycode[i];
    return true;
  }
  
  void setKeyboardLedCallback(void (*cb)(uint16_t, uint8_t)) {
      ledCallback = cb;
  }

  void dispatchKeyboardLeds(uint8_t leds) {
    if (ledCallback) ledCallback(0, leds);
  }
};

struct FakeBluefruit {
  bool connectedResult = false;
  bool connected() { return connectedResult; }
};
extern FakeBluefruit Bluefruit;
FakeBluefruit Bluefruit;

