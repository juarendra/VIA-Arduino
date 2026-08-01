#include <assert.h>
#include <string.h>
#include "VIA_STM32F1_Flash.h"

namespace {

class SimulatedFlash : public via::FlashMemory {
public:
  SimulatedFlash() { memset(mem_, 0xFF, sizeof(mem_)); }

  bool read(uint32_t addr, void* out, uint16_t length) override {
    if (addr < via::stm32f1::kSlotA) return false;
    uint32_t offset = addr - via::stm32f1::kSlotA;
    if (offset + length > kRegionSize) return false;
    memcpy(out, mem_ + offset, length);
    return true;
  }

  bool write(uint32_t addr, const void* data, uint16_t length) override {
    if (addr < via::stm32f1::kSlotA) return false;
    uint32_t offset = addr - via::stm32f1::kSlotA;
    if (offset + length > kRegionSize) return false;
    const uint8_t* src = static_cast<const uint8_t*>(data);
    for (uint16_t i = 0; i < length; ++i) {
      if ((mem_[offset + i] & src[i]) != src[i]) return false;
    }
    for (uint16_t i = 0; i < length; ++i) {
      mem_[offset + i] &= src[i];
    }
    return true;
  }

  bool erasePage(uint32_t addr) override {
    if (addr < via::stm32f1::kSlotA) return false;
    uint32_t offset = addr - via::stm32f1::kSlotA;
    if (offset + via::stm32f1::kPageSize > kRegionSize) return false;
    if (addr & (via::stm32f1::kPageSize - 1)) return false;
    memset(mem_ + offset, 0xFF, via::stm32f1::kPageSize);
    return true;
  }

  bool commit() override { return true; }

  static constexpr uint32_t kRegionSize = 4096;
  uint8_t mem_[kRegionSize];
};

void writeEnvelope(SimulatedFlash& flash, uint32_t slot, uint32_t seq,
                   const uint8_t* payload, uint16_t payloadLen) {
  uint8_t header[12];
  memset(header, 0, sizeof(header));
  uint32_t magic = via::stm32f1::kMagic;
  header[0] = magic & 0xFF;
  header[1] = (magic >> 8) & 0xFF;
  header[2] = (magic >> 16) & 0xFF;
  header[3] = (magic >> 24) & 0xFF;
  header[4] = seq & 0xFF;
  header[5] = (seq >> 8) & 0xFF;
  header[6] = (seq >> 16) & 0xFF;
  header[7] = (seq >> 24) & 0xFF;

  // CRC32 over full payload area
  uint32_t crc = 0xFFFFFFFF;
  for (uint16_t k = 0; k < 256; ++k) {
    uint32_t c = k;
    for (int j = 0; j < 8; ++j) c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
    (void)c;
  }
  // Use a simple crc for test helper; actual crc validated by FlashStorage
  uint8_t full[via::stm32f1::kPayloadSize];
  memset(full, 0xFF, sizeof(full));
  memcpy(full, payload, payloadLen);
  // Compute CRC same as FlashStorage (use generated table at runtime)
  uint32_t tab[256];
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int j = 0; j < 8; ++j) c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
    tab[i] = c;
  }
  for (uint16_t i = 0; i < via::stm32f1::kPayloadSize; ++i)
    crc = tab[(crc ^ full[i]) & 0xFF] ^ (crc >> 8);
  crc ^= 0xFFFFFFFF;

  header[8]  = crc & 0xFF;
  header[9]  = (crc >> 8) & 0xFF;
  header[10] = (crc >> 16) & 0xFF;
  header[11] = (crc >> 24) & 0xFF;

  uint32_t addr = slot + via::stm32f1::kEnvelopeSize;
  for (uint16_t i = 0; i < via::stm32f1::kPayloadSize; ++i)
    flash.write(addr + i, full + i, 1);
  flash.write(slot, header, 12);
  uint8_t marker = via::stm32f1::kCommitMarker;
  flash.write(slot + 12, &marker, 1);
}

void writeHeaderOnly(SimulatedFlash& flash, uint32_t slot, uint32_t seq,
                     const uint8_t* payload, uint16_t payloadLen) {
  uint8_t header[12];
  memset(header, 0, sizeof(header));
  uint32_t magic = via::stm32f1::kMagic;
  header[0] = magic & 0xFF;
  header[1] = (magic >> 8) & 0xFF;
  header[2] = (magic >> 16) & 0xFF;
  header[3] = (magic >> 24) & 0xFF;
  header[4] = seq & 0xFF;
  header[5] = (seq >> 8) & 0xFF;
  header[6] = (seq >> 16) & 0xFF;
  header[7] = (seq >> 24) & 0xFF;

  uint32_t tab[256];
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int j = 0; j < 8; ++j) c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
    tab[i] = c;
  }
  uint8_t full[via::stm32f1::kPayloadSize];
  memset(full, 0xFF, sizeof(full));
  memcpy(full, payload, payloadLen);
  uint32_t crc = 0xFFFFFFFF;
  for (uint16_t i = 0; i < via::stm32f1::kPayloadSize; ++i)
    crc = tab[(crc ^ full[i]) & 0xFF] ^ (crc >> 8);
  crc ^= 0xFFFFFFFF;

  header[8]  = crc & 0xFF;
  header[9]  = (crc >> 8) & 0xFF;
  header[10] = (crc >> 16) & 0xFF;
  header[11] = (crc >> 24) & 0xFF;

  uint32_t addr = slot + via::stm32f1::kEnvelopeSize;
  for (uint16_t i = 0; i < via::stm32f1::kPayloadSize; ++i)
    flash.write(addr + i, full + i, 1);
  flash.write(slot, header, 12);
  // No marker write — simulates power cut before commit marker
}

} // namespace

int main() {
  // Test 1: Empty fresh flash
  {
    SimulatedFlash flash;
    via::stm32f1::FlashStorage storage(flash);
    assert(storage.begin());
    assert(storage.capacity() == via::stm32f1::kPayloadSize);
    uint8_t buf[10];
    memset(buf, 0xCC, sizeof(buf));
    assert(storage.read(0, buf, sizeof(buf)));
    // Erased flash returns 0xFF bytes
    for (int i = 0; i < 10; ++i) assert(buf[i] == 0xFF);
  }

  // Test 2: Write-save-read round-trip
  {
    SimulatedFlash flash;
    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    assert(storage1.erase());
    uint8_t data[50];
    for (uint8_t i = 0; i < 50; ++i) data[i] = i;
    assert(storage1.write(0, data, 50));
    assert(storage1.commit());

    via::stm32f1::FlashStorage storage2(flash);
    assert(storage2.begin());
    uint8_t buf[50];
    memset(buf, 0, sizeof(buf));
    assert(storage2.read(0, buf, 50));
    assert(memcmp(buf, data, 50) == 0);
  }

  // Test 3: Power cut after erase but before write
  {
    SimulatedFlash flash;
    // Pre-populate slot A with valid committed data
    uint8_t dataA[20];
    for (uint8_t i = 0; i < 20; ++i) dataA[i] = i + 10;
    writeEnvelope(flash, via::stm32f1::kSlotA, 5, dataA, 20);

    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    uint8_t buf1[20];
    assert(storage1.read(0, buf1, 20));
    assert(memcmp(buf1, dataA, 20) == 0);

    // Erase slot B (inactive) but power cut before any write
    assert(storage1.erase());
    // slot B is now all 0xFF

    // Power-off: new storage
    via::stm32f1::FlashStorage storage2(flash);
    assert(storage2.begin());
    uint8_t buf2[20];
    assert(storage2.read(0, buf2, 20));
    assert(memcmp(buf2, dataA, 20) == 0);
  }

  // Test 4: Power cut after partial write
  {
    SimulatedFlash flash;
    // Pre-populate slot A with valid committed data
    uint8_t dataA[30];
    for (uint8_t i = 0; i < 30; ++i) dataA[i] = i + 100;
    writeEnvelope(flash, via::stm32f1::kSlotA, 3, dataA, 30);

    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    assert(storage1.erase());
    uint8_t partial[10];
    for (uint8_t i = 0; i < 10; ++i) partial[i] = i + 200;
    assert(storage1.write(0, partial, 10));
    // Power cut before commit

    via::stm32f1::FlashStorage storage2(flash);
    assert(storage2.begin());
    uint8_t buf[30];
    assert(storage2.read(0, buf, 30));
    assert(memcmp(buf, dataA, 30) == 0);
  }

  // Test 5: Power cut before commit marker
  {
    SimulatedFlash flash;
    // Slot A: old committed data
    uint8_t dataA[25];
    for (uint8_t i = 0; i < 25; ++i) dataA[i] = i + 50;
    writeEnvelope(flash, via::stm32f1::kSlotA, 10, dataA, 25);

    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    // Erased slot B, now slot B is inactive (0xFF)
    assert(storage1.erase());

    // Write new data to slot B and write header but NO marker
    uint8_t newData[25];
    for (uint8_t i = 0; i < 25; ++i) newData[i] = i + 60;
    for (uint8_t i = 0; i < 25; ++i)
      flash.write(via::stm32f1::kSlotB + via::stm32f1::kEnvelopeSize + i, newData + i, 1);
    // Fill rest of payload with 0xFF
    for (uint16_t i = 25; i < via::stm32f1::kPayloadSize; ++i) {
      uint8_t ff = 0xFF;
      flash.write(via::stm32f1::kSlotB + via::stm32f1::kEnvelopeSize + i, &ff, 1);
    }
    writeHeaderOnly(flash, via::stm32f1::kSlotB, 11, newData, 25);

    // Power off: new storage
    via::stm32f1::FlashStorage storage2(flash);
    assert(storage2.begin());
    uint8_t buf[25];
    assert(storage2.read(0, buf, 25));
    assert(memcmp(buf, dataA, 25) == 0);
  }

  // Test 6: Power cut after commit marker
  {
    SimulatedFlash flash;
    // Slot A: old committed data (used as starting point after begin+erase)
    uint8_t dataA[15];
    for (uint8_t i = 0; i < 15; ++i) dataA[i] = 0xAA;
    writeEnvelope(flash, via::stm32f1::kSlotA, 1, dataA, 15);

    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    assert(storage1.erase());
    uint8_t dataB[15];
    for (uint8_t i = 0; i < 15; ++i) dataB[i] = 0xBB;
    assert(storage1.write(0, dataB, 15));
    assert(storage1.commit());

    via::stm32f1::FlashStorage storage2(flash);
    assert(storage2.begin());
    uint8_t buf[15];
    assert(storage2.read(0, buf, 15));
    assert(memcmp(buf, dataB, 15) == 0);
  }

  // Test 7: Sequence wraparound
  {
    SimulatedFlash flash;
    uint8_t dataA[10];
    for (uint8_t i = 0; i < 10; ++i) dataA[i] = 0x11;
    writeEnvelope(flash, via::stm32f1::kSlotA, 0xFFFFFFFF, dataA, 10);

    uint8_t dataB[10];
    for (uint8_t i = 0; i < 10; ++i) dataB[i] = 0x22;
    writeEnvelope(flash, via::stm32f1::kSlotB, 0x00000000, dataB, 10);

    via::stm32f1::FlashStorage storage(flash);
    assert(storage.begin());
    uint8_t buf[10];
    assert(storage.read(0, buf, 10));
    // Slot B seq 0 is newer than slot A seq 0xFFFFFFFF (wraparound)
    assert(memcmp(buf, dataB, 10) == 0);
  }

  // Test 8: Both slots corrupt
  {
    SimulatedFlash flash;
    // Write garbage magic to both slots
    uint8_t garbage[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    flash.write(via::stm32f1::kSlotA, garbage, 4);
    flash.write(via::stm32f1::kSlotB, garbage, 4);

    via::stm32f1::FlashStorage storage(flash);
    assert(!storage.begin());
    assert(storage.capacity() == 0);
  }

  // Test 9: Write outside reserved region
  {
    SimulatedFlash flash;
    via::stm32f1::FlashStorage storage(flash);
    assert(storage.begin());
    assert(storage.erase());

    uint8_t data[10];
    // Offset beyond capacity
    assert(!storage.write(via::stm32f1::kPayloadSize - 5, data, 10));
    // Offset exactly at capacity, length 0
    assert(!storage.write(via::stm32f1::kPayloadSize, data, 0));
    // Offset at capacity with length 1
    assert(!storage.write(via::stm32f1::kPayloadSize, data, 1));
    // Valid write still works
    assert(storage.write(0, data, 10));
  }

  // Test 10: Erase overwrites old data only in target slot
  {
    SimulatedFlash flash;
    uint8_t dataA[20];
    for (uint8_t i = 0; i < 20; ++i) dataA[i] = i;
    writeEnvelope(flash, via::stm32f1::kSlotA, 1, dataA, 20);

    uint8_t dataB[20];
    for (uint8_t i = 0; i < 20; ++i) dataB[i] = i + 100;
    writeEnvelope(flash, via::stm32f1::kSlotB, 2, dataB, 20);

    via::stm32f1::FlashStorage storage1(flash);
    assert(storage1.begin());
    uint8_t buf1[20];
    assert(storage1.read(0, buf1, 20));
    assert(memcmp(buf1, dataB, 20) == 0); // Slot B is newer

    // Erase sets up inactive slot (slot A, now seq 1) for new write
    assert(storage1.erase());
    // Active slot (B) should still have its data
    assert(storage1.read(0, buf1, 20));
    assert(memcmp(buf1, dataB, 20) == 0);
  }

  return 0;
}
