#pragma once

#include "VIA_Protocol.h"

#include <stdint.h>
#include <stddef.h>
#include <InternalFileSystem.h>

namespace via {
namespace nrf52 {

class InternalFSStorage : public via::Storage {
 public:
  InternalFSStorage(uint8_t* staging, size_t capacity);
  bool begin();
  size_t capacity() const override;
  bool read(size_t offset, uint8_t* output, size_t length) override;
  bool write(size_t offset, const uint8_t* input, size_t length) override;
  bool commit() override;
  bool erase() override;

 private:
  uint8_t* staging_;
  size_t capacity_;
  uint32_t activeGeneration_;
  bool initialized_;
};

}  // namespace nrf52
}  // namespace via
