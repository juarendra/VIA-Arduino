#include <assert.h>
#include <string.h>

#include "VIA_Arduino.h"

namespace {

class FixedMatrixCallbacks : public via::Callbacks {
 public:
  explicit FixedMatrixCallbacks(uint32_t rowMask) : rowMask_(rowMask) {}

  uint32_t matrixRow(uint8_t) const override { return rowMask_; }

 private:
  uint32_t rowMask_;
};

void assertMatrixPacking(uint8_t columns, uint32_t rowMask,
                         const uint8_t* expected, uint8_t bytesPerRow) {
  uint16_t keymap[29 * 32] = {};
  const uint16_t defaults[29 * 32] = {};
  uint8_t macros[1] = {};
  via::MemoryTransport transport;
  FixedMatrixCallbacks callbacks(rowMask);
  via::Config config = {29, columns, 1, keymap, defaults, macros, 0, 0, 0, 0};
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));

  uint8_t packet[via::kPacketSize];
  memset(packet, 0xA5, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x03;
  packet[2] = 0;
  assert(keyboard.process(packet, 0));

  uint8_t out = 3;
  for (uint8_t row = 0; row < 28 / bytesPerRow; ++row) {
    for (uint8_t byte = 0; byte < bytesPerRow; ++byte) {
      assert(packet[out++] == expected[byte]);
    }
  }
  while (out < 31) assert(packet[out++] == 0);
  assert(packet[31] == 0);
}

void assertMatrixRowOffsetDoesNotWrap() {
  uint16_t keymap[29] = {};
  const uint16_t defaults[29] = {};
  uint8_t macros[1] = {};
  via::MemoryTransport transport;
  FixedMatrixCallbacks callbacks(0xA5);
  via::Config config = {29, 1, 1, keymap, defaults, macros, 0, 0, 0, 0};
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));

  uint8_t packet[via::kPacketSize] = {};
  packet[0] = 0x02;
  packet[1] = 0x03;
  packet[2] = 255;
  assert(keyboard.process(packet, 0));
  for (uint8_t i = 3; i < via::kPacketSize; ++i) assert(packet[i] == 0);
}

}  // namespace

int main() {
  const uint8_t expected8[] = {0x12};
  const uint8_t expected16[] = {0x12, 0x34};
  const uint8_t expected24[] = {0x12, 0x34, 0x56};
  const uint8_t expected32[] = {0x12, 0x34, 0x56, 0x78};
  assertMatrixPacking(8, 0x12, expected8, sizeof(expected8));
  assertMatrixPacking(16, 0x1234, expected16, sizeof(expected16));
  assertMatrixPacking(24, 0x123456, expected24, sizeof(expected24));
  assertMatrixPacking(32, 0x12345678, expected32, sizeof(expected32));
  assertMatrixRowOffsetDoesNotWrap();

  uint16_t keymap[4] = {0x0004, 0x0005, 0x0014, 0x001A};
  const uint16_t defaults[4] = {0x0004, 0x0005, 0x0014, 0x001A};
  uint8_t macros[64] = {};
  uint8_t storageBytes[128] = {};
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  via::Config config = {1, 2, 2, keymap, defaults, macros, sizeof(macros), 2, 1, 1};
  via::Protocol keyboard(config, transport, &storage);
  assert(keyboard.begin(0));

  uint8_t packet[via::kPacketSize] = {};
  packet[0] = 0x01;
  assert(keyboard.process(packet, 10));
  assert(packet[1] == 0 && packet[2] == via::kProtocolVersion);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x05;
  packet[1] = 1;
  packet[2] = 0;
  packet[3] = 1;
  packet[4] = 0x12;
  packet[5] = 0x34;
  assert(keyboard.process(packet, 20));
  assert(keyboard.keycode(1, 0, 1) == 0x1234);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x12;
  packet[1] = 0;
  packet[2] = 6;
  packet[3] = 2;
  assert(keyboard.process(packet, 20));
  assert(packet[4] == 0x12 && packet[5] == 0x34);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x0F;
  packet[1] = 0;
  packet[2] = 0;
  packet[3] = 3;
  packet[4] = 'v';
  packet[5] = 'i';
  packet[6] = 'a';
  assert(keyboard.process(packet, 21));
  assert(keyboard.save());

  uint16_t restored[4] = {};
  uint8_t restoredMacros[64] = {};
  via::Config restoredConfig = {1, 2, 2, restored, defaults, restoredMacros, sizeof(restoredMacros), 2, 1, 1};
  via::MemoryTransport restoredTransport;
  via::Protocol restoredKeyboard(restoredConfig, restoredTransport, &storage);
  assert(restoredKeyboard.begin(0));
  assert(restoredKeyboard.keycode(1, 0, 1) == 0x1234);
  assert(memcmp(restoredMacros, "via", 3) == 0);

  assert(restoredKeyboard.setKeycode(0, 0, 0, 0x5678));
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x06;
  assert(restoredKeyboard.process(packet, 30));
  assert(restoredKeyboard.keycode(0, 0, 0) == defaults[0]);
  assert(restoredMacros[0] == 'v');

  assert(restoredKeyboard.setKeycode(0, 0, 0, 0x5678));
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x10;
  assert(restoredKeyboard.process(packet, 31));
  assert(restoredMacros[0] == 0);
  assert(restoredKeyboard.keycode(0, 0, 0) == 0x5678);
  return 0;
}
