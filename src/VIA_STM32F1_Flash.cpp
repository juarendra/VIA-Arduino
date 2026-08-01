#ifdef STM32F1xx

#include "VIA_STM32F1_Flash.h"

#include <string.h>

namespace via {
namespace stm32f1 {

FlashStorage::FlashStorage(FlashMemory& flash)
    : flash_(flash), activeSlot_(kSlotA), provisionalSlot_(kSlotB),
      activeSeq_(0), newSeq_(0), activeValid_(false), provisionalReady_(false) {}

uint32_t FlashStorage::crc32(const uint8_t* data, size_t len) const {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b)
      crc = (crc & 1) ? 0xEDB88320 ^ (crc >> 1) : crc >> 1;
  }
  return crc ^ 0xFFFFFFFF;
}

bool FlashStorage::validateSlot(uint32_t slotAddr, uint32_t& seq, bool& empty) {
  uint8_t buf[13]; // magic(4) + seq(4) + crc(4) + marker(1)
  if (!flash_.read(slotAddr, buf, 13)) {
    empty = false;
    return false;
  }

  // All 0xFF means erased empty slot
  bool allFF = true;
  for (int i = 0; i < 13; ++i) {
    if (buf[i] != 0xFF) { allFF = false; break; }
  }
  if (allFF) {
    empty = true;
    return false;
  }

  empty = false;

  uint32_t magic = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8)
                 | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
  if (magic != kMagic) return false;

  if (buf[12] != kCommitMarker) return false;

  uint8_t payload[kPayloadSize]; // ponytail: stack 2KB, ok for host test; real HW validates in-place
  if (!flash_.read(slotAddr + kEnvelopeSize, payload, kPayloadSize)) return false;

  uint32_t expected = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8)
                    | ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
  uint32_t computed = crc32(payload, kPayloadSize);
  if (computed != expected) return false;

  seq = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8)
      | ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
  return true;
}

bool FlashStorage::begin() {
  uint32_t seqA, seqB;
  bool emptyA = false, emptyB = false;
  bool validA = validateSlot(kSlotA, seqA, emptyA);
  bool validB = validateSlot(kSlotB, seqB, emptyB);

  if (!validA && !validB) {
    if (emptyA) {
      activeSlot_ = kSlotA;
      activeSeq_ = 0;
      activeValid_ = true;
    } else if (emptyB) {
      activeSlot_ = kSlotB;
      activeSeq_ = 0;
      activeValid_ = true;
    } else {
      activeValid_ = false;
      return false;
    }
  } else if (validA && validB) {
    int32_t diff = (int32_t)(seqA - seqB);
    if (diff > 0) {
      activeSlot_ = kSlotA;
      activeSeq_ = seqA;
    } else {
      activeSlot_ = kSlotB;
      activeSeq_ = seqB;
    }
    activeValid_ = true;
  } else if (validA) {
    activeSlot_ = kSlotA;
    activeSeq_ = seqA;
    activeValid_ = true;
  } else {
    activeSlot_ = kSlotB;
    activeSeq_ = seqB;
    activeValid_ = true;
  }

  return activeValid_;
}

size_t FlashStorage::capacity() const {
  return activeValid_ ? kPayloadSize : 0;
}

bool FlashStorage::read(size_t offset, uint8_t* output, size_t length) {
  if (!activeValid_ || length == 0) return false;
  if (offset > kPayloadSize || length > kPayloadSize - offset) return false;
  return flash_.read(activeSlot_ + kEnvelopeSize + (uint32_t)offset, output, (uint16_t)length);
}

bool FlashStorage::write(size_t offset, const uint8_t* input, size_t length) {
  if (!provisionalReady_ || length == 0) return false;
  if (offset > kPayloadSize || length > kPayloadSize - offset) return false;
  return flash_.write(provisionalSlot_ + kEnvelopeSize + (uint32_t)offset, input, (uint16_t)length);
}

bool FlashStorage::commit() {
  if (!provisionalReady_) return false;

  uint8_t payload[kPayloadSize]; // ponytail: stack 2KB, ok for host test
  if (!flash_.read(provisionalSlot_ + kEnvelopeSize, payload, kPayloadSize)) return false;
  uint32_t crc = crc32(payload, kPayloadSize);

  uint8_t header[12];
  header[0]  = kMagic & 0xFF;
  header[1]  = (kMagic >> 8) & 0xFF;
  header[2]  = (kMagic >> 16) & 0xFF;
  header[3]  = (kMagic >> 24) & 0xFF;
  header[4]  = newSeq_ & 0xFF;
  header[5]  = (newSeq_ >> 8) & 0xFF;
  header[6]  = (newSeq_ >> 16) & 0xFF;
  header[7]  = (newSeq_ >> 24) & 0xFF;
  header[8]  = crc & 0xFF;
  header[9]  = (crc >> 8) & 0xFF;
  header[10] = (crc >> 16) & 0xFF;
  header[11] = (crc >> 24) & 0xFF;

  if (!flash_.write(provisionalSlot_, header, 12)) return false;
  uint8_t marker = kCommitMarker;
  if (!flash_.write(provisionalSlot_ + 12, &marker, 1)) return false;

  if (!flash_.commit()) return false;

  activeSlot_ = provisionalSlot_;
  activeSeq_ = newSeq_;
  provisionalReady_ = false;
  return true;
}

bool FlashStorage::erase() {
  if (!activeValid_) return false;

  provisionalSlot_ = (activeSlot_ == kSlotA) ? kSlotB : kSlotA;
  newSeq_ = activeSeq_ + 1;

  if (!flash_.erasePage(provisionalSlot_)) return false;
  if (!flash_.erasePage(provisionalSlot_ + kPageSize)) return false;

  provisionalReady_ = true;
  return true;
}

}  // namespace stm32f1
}  // namespace via

#endif  // STM32F1xx
