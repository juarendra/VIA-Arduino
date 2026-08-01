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

class RecordingCallbacks : public via::Callbacks {
 public:
  RecordingCallbacks()
      : indicationValue(0), indicationCalls(0), layoutValue(0), layoutCalls(0) {}

  void deviceIndication(uint8_t value) override {
    indicationValue = value;
    ++indicationCalls;
  }
  void layoutOptionsChanged(uint32_t value) override {
    layoutValue = value;
    ++layoutCalls;
  }

  uint8_t indicationValue;
  uint8_t indicationCalls;
  uint32_t layoutValue;
  uint8_t layoutCalls;
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

void assertEncoderRejected(via::Protocol& keyboard, uint8_t command,
                           uint8_t layer, uint8_t encoder, uint8_t clockwise) {
  uint8_t packet[via::kPacketSize] = {};
  packet[0] = command;
  packet[1] = layer;
  packet[2] = encoder;
  packet[3] = clockwise;
  assert(!keyboard.process(packet, 0));
  assert(packet[0] == 0xFF);
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
  uint16_t encoderMap[4] = {};
  const uint16_t defaultEncoderMap[4] = {0x0101, 0x0102, 0x0201, 0x0202};
  uint8_t macros[64] = {};
  uint8_t storageBytes[128] = {};
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  RecordingCallbacks callbacks;
  via::Config config = {1, 2, 2, keymap, defaults, macros, sizeof(macros), 2, 1, 1,
                        0x01020304UL, 1, encoderMap, defaultEncoderMap};
  via::Protocol keyboard(config, transport, &storage, nullptr, &callbacks);
  assert(keyboard.begin(0));
  assert(keyboard.layoutOptions() == 0x01020304UL);
  assert(keyboard.encoderKeycode(1, 0, 0) == 0x0201);

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
  packet[0] = 0x03;
  packet[1] = 0x02;
  packet[2] = 0x12;
  packet[3] = 0x34;
  packet[4] = 0x56;
  packet[5] = 0x78;
  assert(keyboard.process(packet, 20));
  assert(keyboard.layoutOptions() == 0x12345678UL);
  assert(callbacks.layoutCalls == 1);
  assert(callbacks.layoutValue == 0x12345678UL);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x02;
  assert(keyboard.process(packet, 20));
  assert(packet[2] == 0x12 && packet[3] == 0x34);
  assert(packet[4] == 0x56 && packet[5] == 0x78);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x03;
  packet[1] = 0x05;
  packet[2] = 4;
  assert(keyboard.process(packet, 20));
  assert(callbacks.indicationCalls == 1);
  assert(callbacks.indicationValue == 4);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x15;
  packet[1] = 1;
  packet[2] = 0;
  packet[3] = 1;
  packet[4] = 0xBE;
  packet[5] = 0xEF;
  assert(keyboard.process(packet, 20));
  assert(encoderMap[0] == 0x0101 && encoderMap[1] == 0x0102);
  assert(encoderMap[2] == 0x0201 && encoderMap[3] == 0xBEEF);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x14;
  packet[1] = 1;
  packet[2] = 0;
  packet[3] = 1;
  assert(keyboard.process(packet, 20));
  assert(packet[4] == 0xBE && packet[5] == 0xEF);

  assertEncoderRejected(keyboard, 0x14, 2, 0, 0);
  assertEncoderRejected(keyboard, 0x14, 0, 1, 0);
  assertEncoderRejected(keyboard, 0x14, 0, 0, 2);
  assertEncoderRejected(keyboard, 0x15, 0, 0, 2);

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
  uint16_t restoredEncoderMap[4] = {};
  uint8_t restoredMacros[64] = {};
  via::Config restoredConfig = {
      1, 2, 2, restored, defaults, restoredMacros, sizeof(restoredMacros), 2, 1, 1,
      0x01020304UL, 1, restoredEncoderMap, defaultEncoderMap};
  via::MemoryTransport restoredTransport;
  via::Protocol restoredKeyboard(restoredConfig, restoredTransport, &storage);
  assert(restoredKeyboard.begin(0));
  assert(restoredKeyboard.keycode(1, 0, 1) == 0x1234);
  assert(restoredKeyboard.layoutOptions() == 0x12345678UL);
  assert(restoredKeyboard.encoderKeycode(1, 0, 1) == 0xBEEF);
  assert(memcmp(restoredMacros, "via", 3) == 0);

  assert(restoredKeyboard.setKeycode(0, 0, 0, 0x5678));
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x06;
  assert(restoredKeyboard.process(packet, 30));
  assert(restoredKeyboard.keycode(0, 0, 0) == defaults[0]);
  assert(memcmp(restoredEncoderMap, defaultEncoderMap,
                sizeof(restoredEncoderMap)) == 0);
  assert(restoredMacros[0] == 'v');

  assert(restoredKeyboard.setKeycode(0, 0, 0, 0x5678));
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x10;
  assert(restoredKeyboard.process(packet, 31));
  assert(restoredMacros[0] == 0);
  assert(restoredKeyboard.keycode(0, 0, 0) == 0x5678);
  return 0;
}
