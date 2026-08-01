#pragma once

#include "VIA_Protocol.h"

#include <stddef.h>
#include <stdint.h>

namespace via {

class FlashMemory {
 public:
  virtual ~FlashMemory() {}
  virtual bool read(uint32_t addr, void* out, uint16_t length) = 0;
  virtual bool write(uint32_t addr, const void* data, uint16_t length) = 0;
  virtual bool erasePage(uint32_t addr) = 0;
  virtual bool commit() = 0;
};

namespace stm32f1 {

constexpr uint32_t kSlotA = 0x0801F000;
constexpr uint32_t kSlotB = 0x0801F800;
constexpr uint32_t kSlotSize = 2048;
constexpr uint32_t kPageSize = 1024;
constexpr uint16_t kEnvelopeSize = 16;
constexpr uint16_t kPayloadSize = 2032;
constexpr uint32_t kMagic = 0xA5F103CB;
constexpr uint8_t kCommitMarker = 0x5A;

class FlashStorage : public via::Storage {
 public:
  explicit FlashStorage(FlashMemory& flash);

  bool begin();
  size_t capacity() const override;
  bool read(size_t offset, uint8_t* output, size_t length) override;
  bool write(size_t offset, const uint8_t* input, size_t length) override;
  bool commit() override;
  bool erase() override;

 private:
  uint32_t crc32(const uint8_t* data, size_t len) const;
  bool validateSlot(uint32_t slotAddr, uint32_t& seq, bool& empty);

  FlashMemory& flash_;
  uint32_t activeSlot_;
  uint32_t provisionalSlot_;
  uint32_t activeSeq_;
  uint32_t newSeq_;
  bool activeValid_;
  bool provisionalReady_;
};

}  // namespace stm32f1
}  // namespace via
