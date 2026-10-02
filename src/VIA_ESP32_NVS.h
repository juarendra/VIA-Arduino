#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Protocol.h"
#include <Preferences.h>
#include <string.h>

namespace via {
namespace esp32 {

/* NVS-backed VIA storage for generic ESP32 boards (WROOM-32, S3).
 *
 * The real espressif/arduino-esp32 Preferences API only offers
 * getBytes(key, buf, maxLen) and putBytes(key, value, len) - there is no
 * offset parameter. This adapter keeps a full 4096-byte shadow copy in RAM:
 * read/write operate on the shadow with bounds checks, and commit()
 * persists the whole shadow as one NVS blob. Erased/modified bytes stay
 * provisional (in RAM) until commit(), so a power failure mid-reset falls
 * back to the last durable record. */
class NVSStorage : public via::Storage {
 public:
  static constexpr size_t kCapacity = 4096;

  NVSStorage() { memset(shadow_, 0xFF, sizeof(shadow_)); }

  bool begin(const char* ns = "via") {
    if (!prefs_.begin(ns, false)) return false;
    ns_ = ns;
    size_t loaded = prefs_.getBytes(ksKey_, shadow_, sizeof(shadow_));
    if (loaded != sizeof(shadow_)) {
      // Missing or truncated blob: start from an erased state.
      memset(shadow_, 0xFF, sizeof(shadow_));
    }
    return true;
  }

  size_t capacity() const override { return kCapacity; }

  bool read(size_t offset, uint8_t* output, size_t length) override {
    if (length == 0) return true;
    if (offset > kCapacity || length > kCapacity - offset) return false;
    memcpy(output, shadow_ + offset, length);
    return true;
  }

  bool write(size_t offset, const uint8_t* input, size_t length) override {
    if (length == 0) return true;
    if (offset > kCapacity || length > kCapacity - offset) return false;
    memcpy(shadow_ + offset, input, length);
    return true;
  }

  bool commit() override {
    return prefs_.putBytes(ksKey_, shadow_, sizeof(shadow_)) == sizeof(shadow_);
  }

  /* Provisions an erased state in the shadow. It only becomes durable when
   * commit() follows, matching the Storage atomic-commit contract. */
  bool erase() override {
    memset(shadow_, 0xFF, sizeof(shadow_));
    return true;
  }

 private:
  static constexpr const char* ksKey_ = "via_payload";
  Preferences prefs_;
  const char* ns_ = "via";
  uint8_t shadow_[kCapacity];
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
