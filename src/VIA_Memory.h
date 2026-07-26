#pragma once

#include "VIA_Protocol.h"

#include <string.h>

namespace via {

/* Useful for examples, unit tests, and boards whose persistent storage adapter
 * is still being developed. It intentionally loses data at power-off. */
class MemoryStorage : public Storage {
 public:
  MemoryStorage(uint8_t* buffer, size_t length) : buffer_(buffer), length_(length) {
    if (buffer_) memset(buffer_, 0xFF, length_);
  }

  size_t capacity() const override { return length_; }
  bool read(size_t offset, uint8_t* output, size_t length) override {
    if (!buffer_ || offset > length_ || length > length_ - offset) return false;
    memcpy(output, buffer_ + offset, length);
    return true;
  }
  bool write(size_t offset, const uint8_t* input, size_t length) override {
    if (!buffer_ || offset > length_ || length > length_ - offset) return false;
    memcpy(buffer_ + offset, input, length);
    return true;
  }
  bool commit() override { return true; }
  bool erase() override {
    if (!buffer_) return false;
    memset(buffer_, 0xFF, length_);
    return true;
  }

 private:
  uint8_t* buffer_;
  size_t length_;
};

/* A one-packet test transport. Replace this with a native-USB Raw HID adapter
 * on actual hardware. */
class MemoryTransport : public Transport {
 public:
  MemoryTransport() : requestReady_(false), responseReady_(false) {
    memset(request_, 0, sizeof(request_));
    memset(response_, 0, sizeof(response_));
  }
  bool receive(uint8_t packet[kPacketSize]) override {
    if (!requestReady_) return false;
    memcpy(packet, request_, kPacketSize);
    requestReady_ = false;
    return true;
  }
  bool send(const uint8_t packet[kPacketSize]) override {
    memcpy(response_, packet, kPacketSize);
    responseReady_ = true;
    return true;
  }
  void inject(const uint8_t packet[kPacketSize]) {
    memcpy(request_, packet, kPacketSize);
    requestReady_ = true;
  }
  bool takeResponse(uint8_t packet[kPacketSize]) {
    if (!responseReady_) return false;
    memcpy(packet, response_, kPacketSize);
    responseReady_ = false;
    return true;
  }

 private:
  uint8_t request_[kPacketSize];
  uint8_t response_[kPacketSize];
  bool requestReady_;
  bool responseReady_;
};

}  // namespace via
