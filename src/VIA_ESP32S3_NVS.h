#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Protocol.h"
#include <Preferences.h>
#include <string.h>

namespace via {
namespace esp32s3 {

class NVSStorage : public via::Storage {
 public:
  NVSStorage() {}

  bool begin(const char* ns = "via") {
    return prefs_.begin(ns, false);
  }

  size_t capacity() const override { return 4096; }
  bool read(size_t offset, uint8_t* output, size_t length) override {
    if (length == 0) return true;
    return prefs_.getBytes(ksKey_, output + offset, length, offset) == length;
  }
  bool write(size_t offset, const uint8_t* input, size_t length) override {
    if (length == 0) return true;
    return prefs_.putBytes(ksKey_, input + offset, length, offset) == length;
  }
  bool commit() override {
    prefs_.end();
    return prefs_.begin(ns_, false);
  }
  bool erase() override {
    prefs_.clear();
    prefs_.end();
    return prefs_.begin(ns_, false);
  }

 private:
  static constexpr const char* ksKey_ = "via_payload";
  Preferences prefs_;
  const char* ns_ = "via";
};

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
