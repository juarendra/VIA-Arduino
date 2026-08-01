#pragma once

#include <stdint.h>

namespace via {

struct KeyboardReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
};

class KeyboardCallbacks {
 public:
  virtual void hostLedsChanged(uint8_t /*leds*/) {}
  virtual void bootloaderRequested() {}
 protected:
  ~KeyboardCallbacks() = default;
};

class KeyboardHID {
 public:
  virtual bool configured() const = 0;
  virtual bool send(const KeyboardReport& r) = 0;
  virtual bool sendComplete() = 0;
  virtual bool takeHostLeds(uint8_t& leds) = 0;
  virtual bool suspended() const = 0;
  virtual bool remoteWakeupAllowed() const = 0;
  virtual bool remoteWakeup() = 0;

 protected:
  ~KeyboardHID() = default;
};

}  // namespace via
