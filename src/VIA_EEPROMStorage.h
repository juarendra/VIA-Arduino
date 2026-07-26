#pragma once

#if __has_include(<EEPROM.h>)

#include <EEPROM.h>

#include "VIA_Protocol.h"

namespace via {

/* EEPROM-compatible persistence adapter. It is suitable for RP2040 Arduino
 * EEPROM emulation and similar cores. Production flash ports that need power
 * loss atomicity should implement Storage with alternating slots instead. */
class EEPROMStorage : public Storage {
 public:
  explicit EEPROMStorage(size_t length) : length_(length), started_(false) {}

  bool begin() {
    EEPROM.begin(length_);
    started_ = true;
    return true;
  }
  size_t capacity() const override { return length_; }
  bool read(size_t offset, uint8_t* output, size_t length) override {
    if (!started_ || offset > length_ || length > length_ - offset) return false;
    for (size_t i = 0; i < length; ++i) output[i] = EEPROM.read(offset + i);
    return true;
  }
  bool write(size_t offset, const uint8_t* input, size_t length) override {
    if (!started_ || offset > length_ || length > length_ - offset) return false;
    for (size_t i = 0; i < length; ++i) EEPROM.write(offset + i, input[i]);
    return true;
  }
  bool commit() override { return started_ && EEPROM.commit(); }
  bool erase() override {
    if (!started_) return false;
    for (size_t i = 0; i < length_; ++i) EEPROM.write(i, 0xFF);
    return EEPROM.commit();
  }

 private:
  size_t length_;
  bool started_;
};

}  // namespace via

#else

#error "VIA_EEPROMStorage requires an Arduino core that provides EEPROM.h"

#endif
