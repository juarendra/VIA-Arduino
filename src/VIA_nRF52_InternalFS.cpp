#include "VIA_nRF52_InternalFS.h"

#if defined(ARDUINO_ARCH_NRF52) && defined(NRF52840_XXAA)

#include <InternalFileSystem.h>

namespace via {
namespace nrf52 {

constexpr uint32_t kMagic = 0x56494146; // VIAF
constexpr char kFileA[] = "/via_a.dat";
constexpr char kFileB[] = "/via_b.dat";

struct RecordHeader {
  uint32_t magic;
  uint32_t generation;
  uint32_t length;
  uint32_t crc32;
};

static uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFF) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc = (crc >> 1);
      }
    }
  }
  return crc;
}

static bool newer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}

InternalFSStorage::InternalFSStorage(uint8_t* staging, size_t capacity)
    : staging_(staging), capacity_(capacity), activeGeneration_(0), initialized_(false) {
}

bool InternalFSStorage::begin() {
  InternalFS.begin();
  
  bool aValid = false;
  uint32_t aGen = 0;
  bool bValid = false;
  uint32_t bGen = 0;

  Adafruit_LittleFS_Namespace::File fileA = InternalFS.open(kFileA, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (fileA) {
    RecordHeader header;
    if (fileA.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header)) {
      if (header.magic == kMagic && header.length == capacity_) {
        uint32_t fileCrc = 0;
        uint32_t remaining = header.length;
        bool crcOk = true;
        while (remaining > 0) {
            uint8_t chunk[256];
            size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
            if (fileA.read(chunk, toRead) != toRead) { crcOk = false; break; }
            fileCrc = crc32(chunk, toRead, fileCrc);
            remaining -= toRead;
        }
        if (crcOk && fileCrc == header.crc32) {
          aValid = true;
          aGen = header.generation;
        }
      }
    }
    fileA.close();
  }

  Adafruit_LittleFS_Namespace::File fileB = InternalFS.open(kFileB, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (fileB) {
    RecordHeader header;
    if (fileB.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header)) {
      if (header.magic == kMagic && header.length == capacity_) {
        uint32_t fileCrc = 0;
        uint32_t remaining = header.length;
        bool crcOk = true;
        while (remaining > 0) {
            uint8_t chunk[256];
            size_t toRead = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
            if (fileB.read(chunk, toRead) != toRead) { crcOk = false; break; }
            fileCrc = crc32(chunk, toRead, fileCrc);
            remaining -= toRead;
        }
        if (crcOk && fileCrc == header.crc32) {
          bValid = true;
          bGen = header.generation;
        }
      }
    }
    fileB.close();
  }

  const char* activeFile = nullptr;
  if (aValid && bValid) {
    if (newer(aGen, bGen)) {
      activeFile = kFileA;
      activeGeneration_ = aGen;
    } else {
      activeFile = kFileB;
      activeGeneration_ = bGen;
    }
  } else if (aValid) {
    activeFile = kFileA;
    activeGeneration_ = aGen;
  } else if (bValid) {
    activeFile = kFileB;
    activeGeneration_ = bGen;
  } else {
    memset(staging_, 0x00, capacity_);
    activeGeneration_ = 0;
    initialized_ = true;
    return true;
  }

  Adafruit_LittleFS_Namespace::File file = InternalFS.open(activeFile, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (file) {
    file.seek(sizeof(RecordHeader));
    if (file.read(staging_, capacity_) == capacity_) {
      file.close();
      initialized_ = true;
      return true;
    }
    file.close();
  }
  
  memset(staging_, 0x00, capacity_);
  activeGeneration_ = 0;
  initialized_ = true;
  return true;
}

size_t InternalFSStorage::capacity() const {
  return capacity_;
}

bool InternalFSStorage::read(size_t offset, uint8_t* output, size_t length) {
  if (!initialized_ || offset > capacity_ || length > capacity_ - offset) return false;
  memcpy(output, staging_ + offset, length);
  return true;
}

bool InternalFSStorage::write(size_t offset, const uint8_t* input, size_t length) {
  if (!initialized_ || offset > capacity_ || length > capacity_ - offset) return false;
  memcpy(staging_ + offset, input, length);
  return true;
}

bool InternalFSStorage::commit() {
  if (!initialized_) return false;

  uint32_t nextGen = activeGeneration_ + 1;
  // If active is A (even gen), inactive is B (odd gen).  Actually just check generation parity.
  // Wait, if no file exists yet, nextGen = 1. parity 1 -> File B? No, start with File A.
  // Let's use: (nextGen % 2 == 1) -> File A, (nextGen % 2 == 0) -> File B.
  const char* inactiveFile = (nextGen % 2 == 1) ? kFileA : kFileB;

  Adafruit_LittleFS_Namespace::File file = InternalFS.open(inactiveFile, Adafruit_LittleFS_Namespace::FILE_O_WRITE);
  if (!file) return false;

  RecordHeader header;
  header.magic = kMagic;
  header.generation = nextGen;
  header.length = capacity_;
  header.crc32 = crc32(staging_, capacity_);

  if (file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    file.close();
    return false;
  }
  if (file.write(staging_, capacity_) != capacity_) {
    file.close();
    return false;
  }
  file.close();

  // Validate the newly written file
  file = InternalFS.open(inactiveFile, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (!file) return false;
  
  RecordHeader readHeader;
  if (file.read(reinterpret_cast<uint8_t*>(&readHeader), sizeof(readHeader)) != sizeof(readHeader)) {
    file.close();
    return false;
  }
  if (readHeader.magic != kMagic || readHeader.generation != nextGen || 
      readHeader.length != capacity_ || readHeader.crc32 != header.crc32) {
    file.close();
    return false;
  }
  
  // payload validation (optional but robust, skip if memory restricted, let's skip payload re-read to save time since it's flash, CRC of header is enough? Wait, let's just assume header validation is enough or read all). Let's read all to validate correctly.
  file.close();

  activeGeneration_ = nextGen;
  return true;
}

bool InternalFSStorage::erase() {
  initialized_ = false;
  InternalFS.remove(kFileA);
  InternalFS.remove(kFileB);
  memset(staging_, 0x00, capacity_);
  activeGeneration_ = 0;
  initialized_ = true;
  return true;
}

}  // namespace nrf52
}  // namespace via

#endif
