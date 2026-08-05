#pragma once
#if defined(ARDUINO_ARCH_NRF52) && defined(NRF52840_XXAA)
#include <Arduino.h>
#include <bluefruit.h>
#include "VIA_HID.h"

namespace via { namespace nrf52 {

class BLEKeyboardHID : public via::KeyboardHID {
  BLEHidAdafruit& service;
  bool ledsPending;
  uint8_t pendingLeds;

  static BLEKeyboardHID* activeAdapter;
  static void ledCallback(uint16_t conn_hdl, uint8_t leds) {
    (void)conn_hdl;
    if (activeAdapter) {
      activeAdapter->pendingLeds = leds;
      activeAdapter->ledsPending = true;
    }
  }

 public:
  explicit BLEKeyboardHID(BLEHidAdafruit& s) : service(s), ledsPending(false), pendingLeds(0) {}

  bool begin() {
    if (activeAdapter) return false;
    activeAdapter = this;
    service.setKeyboardLedCallback(ledCallback);
    return true;
  }

  bool configured() const override { return Bluefruit.connected(); }
  
  bool send(const via::KeyboardReport& report) override {
    return service.keyboardReport(report.modifiers, report.keys);
  }
  
  bool sendComplete() override { return true; }
  
  bool takeHostLeds(uint8_t& leds) override {
    if (ledsPending) {
      leds = pendingLeds;
      ledsPending = false;
      return true;
    }
    return false;
  }
  
  bool suspended() const override { return !Bluefruit.connected(); }
  bool remoteWakeupAllowed() const override { return false; }
  bool remoteWakeup() override { return false; }
};

BLEKeyboardHID* BLEKeyboardHID::activeAdapter = nullptr;

}}
#endif
