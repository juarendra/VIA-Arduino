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
      : indicationValue(0), indicationCalls(0), layoutValue(0), layoutCalls(0),
        changeCalls(0) {}

  void deviceIndication(uint8_t value) override {
    indicationValue = value;
    ++indicationCalls;
  }
  void layoutOptionsChanged(uint32_t value) override {
    layoutValue = value;
    ++layoutCalls;
  }
  void changed() override { ++changeCalls; }

  uint8_t indicationValue;
  uint8_t indicationCalls;
  uint32_t layoutValue;
  uint8_t layoutCalls;
  uint8_t changeCalls;
};

class SecurityCallbacks : public via::Callbacks {
 public:
  SecurityCallbacks() : matrixCalls(0), bootloaderCalls(0) {}

  uint32_t matrixRow(uint8_t) const override {
    ++matrixCalls;
    return 0xA5;
  }
  void bootloaderJump() override { ++bootloaderCalls; }

  mutable uint8_t matrixCalls;
  uint8_t bootloaderCalls;
};

class FailFirstTransport : public via::Transport {
 public:
  FailFirstTransport()
      : requestCount(0), delivered(0), receiveCalls(0), sendCalls(0) {
    memset(requests, 0, sizeof(requests));
    memset(sent, 0, sizeof(sent));
  }

  bool receive(uint8_t packet[via::kPacketSize]) override {
    ++receiveCalls;
    if (delivered == requestCount) return false;
    memcpy(packet, requests[delivered++], via::kPacketSize);
    return true;
  }
  bool send(const uint8_t packet[via::kPacketSize]) override {
    memcpy(sent[sendCalls], packet, via::kPacketSize);
    ++sendCalls;
    return sendCalls != 1;
  }
  void queue(const uint8_t packet[via::kPacketSize]) {
    assert(requestCount < 2);
    memcpy(requests[requestCount++], packet, via::kPacketSize);
  }

  uint8_t requests[2][via::kPacketSize];
  uint8_t sent[3][via::kPacketSize];
  uint8_t requestCount;
  uint8_t delivered;
  uint8_t receiveCalls;
  uint8_t sendCalls;
};

class LegacyIndicationCallbacks : public via::Callbacks {
 public:
  LegacyIndicationCallbacks() : calls(0) {}

  void deviceIndication(bool enabled) override {
    values[calls++] = enabled;
  }

  bool values[2];
  uint8_t calls;
};

class RecordingStorage : public via::Storage {
 public:
  RecordingStorage() : accesses(0), reads(0), writes(0), commits(0), erases(0) {}

  size_t capacity() const override {
    ++accesses;
    return sizeof(bytes);
  }
  bool read(size_t, uint8_t*, size_t) override {
    ++accesses;
    ++reads;
    return false;
  }
  bool write(size_t, const uint8_t*, size_t) override {
    ++accesses;
    ++writes;
    return true;
  }
  bool commit() override {
    ++accesses;
    ++commits;
    return true;
  }
  bool erase() override {
    ++accesses;
    ++erases;
    return true;
  }
  void reset() {
    accesses = 0;
    reads = 0;
    writes = 0;
    commits = 0;
    erases = 0;
  }

  mutable uint8_t accesses;
  uint8_t reads;
  uint8_t writes;
  uint8_t commits;
  uint8_t erases;
  uint8_t bytes[64];
};

class FailingReadStorage : public via::Storage {
 public:
  FailingReadStorage(uint8_t* buffer, size_t length)
      : buffer_(buffer), length_(length), reads_(0), failRead_(0) {
    memset(buffer_, 0xFF, length_);
  }

  size_t capacity() const override { return length_; }
  bool read(size_t offset, uint8_t* output, size_t length) override {
    ++reads_;
    if (reads_ == failRead_ || offset > length_ || length > length_ - offset) {
      return false;
    }
    memcpy(output, buffer_ + offset, length);
    return true;
  }
  bool write(size_t offset, const uint8_t* input, size_t length) override {
    if (offset > length_ || length > length_ - offset) return false;
    memcpy(buffer_ + offset, input, length);
    return true;
  }
  bool commit() override { return true; }
  bool erase() override {
    memset(buffer_, 0xFF, length_);
    return true;
  }
  void failRead(uint8_t call) {
    reads_ = 0;
    failRead_ = call;
  }

 private:
  uint8_t* buffer_;
  size_t length_;
  uint8_t reads_;
  uint8_t failRead_;
};

class ChannelSevenCustomValue : public via::CustomValue {
 public:
  ChannelSevenCustomValue() : setCalls(0), getCalls(0), saveCalls(0) {}

  bool set(uint8_t packet[via::kPacketSize]) override {
    ++setCalls;
    return packet[1] == 7;
  }
  bool get(uint8_t packet[via::kPacketSize]) override {
    ++getCalls;
    return packet[1] == 7;
  }
  bool save(uint8_t packet[via::kPacketSize]) override {
    ++saveCalls;
    return packet[1] == 7;
  }

  uint8_t setCalls;
  uint8_t getCalls;
  uint8_t saveCalls;
};

class LegacyCustomValue : public via::CustomValue {
 public:
  LegacyCustomValue() : setCalls(0), getCalls(0) {}

  bool set(uint8_t packet[via::kPacketSize]) override {
    ++setCalls;
    return packet[1] == 2;
  }
  bool get(uint8_t packet[via::kPacketSize]) override {
    ++getCalls;
    return packet[1] == 2;
  }

  uint8_t setCalls;
  uint8_t getCalls;
};

class OversizedCustomValue : public via::CustomValue {
 public:
  bool set(uint8_t[via::kPacketSize]) override { return true; }
  bool get(uint8_t[via::kPacketSize]) override { return true; }
  bool save(uint8_t[via::kPacketSize]) override { return true; }
  size_t stateSize() const override { return via::kMaxCustomStateSize + 1; }
};

class StoredCustomValue : public via::CustomValue {
 public:
  explicit StoredCustomValue(uint8_t initialValue, bool changingSave = false,
                             bool rejectLoad = false)
      : value(initialValue), saveCalls(0), loadCalls(0),
        changingSave_(changingSave), rejectLoad_(rejectLoad) {}

  bool set(uint8_t[via::kPacketSize]) override { return true; }
  bool get(uint8_t[via::kPacketSize]) override { return true; }
  size_t stateSize() const override { return 1; }
  bool saveState(uint8_t* state, size_t size) const override {
    if (size != 1) return false;
    ++saveCalls;
    state[0] = changingSave_ ? saveCalls : value;
    return true;
  }
  bool loadState(const uint8_t* state, size_t size) override {
    if (size != 1) return false;
    ++loadCalls;
    if (rejectLoad_) return false;
    value = state[0];
    return true;
  }

  uint8_t value;
  mutable uint8_t saveCalls;
  uint8_t loadCalls;

 private:
  bool changingSave_;
  bool rejectLoad_;
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

void assertLegacyIndicationCallback() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  uint8_t macros[1] = {};
  via::MemoryTransport transport;
  LegacyIndicationCallbacks callbacks;
  via::Config config = {1, 1, 1, keymap, defaults, macros, 0, 0, 0, 0};
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));

  uint8_t packet[via::kPacketSize] = {};
  packet[0] = 0x03;
  packet[1] = 0x05;
  assert(keyboard.process(packet, 0));
  packet[2] = 4;
  assert(keyboard.process(packet, 0));
  assert(callbacks.calls == 2);
  assert(!callbacks.values[0]);
  assert(callbacks.values[1]);
}

void assertCustomValueRouting() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  ChannelSevenCustomValue customValue;
  uint8_t loadBuffer[6];
  via::Config config = {1, 1, 1, keymap, defaults};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &customValue);
  assert(keyboard.begin(0));
  storage.reset();

  const uint8_t commands[] = {0x07, 0x08, 0x09};
  uint8_t packet[via::kPacketSize] = {};
  for (uint8_t i = 0; i < sizeof(commands); ++i) {
    packet[0] = commands[i];
    packet[1] = 7;
    assert(keyboard.process(packet, 0));
  }
  assert(customValue.setCalls == 1);
  assert(customValue.getCalls == 1);
  assert(customValue.saveCalls == 1);
  assert(storage.commits == 1);

  for (uint8_t i = 0; i < sizeof(commands); ++i) {
    packet[0] = commands[i];
    packet[1] = 8;
    assert(!keyboard.process(packet, 0));
    assert(packet[0] == 0xFF);
  }
  assert(customValue.setCalls == 2);
  assert(customValue.getCalls == 2);
  assert(customValue.saveCalls == 2);
  assert(storage.commits == 1);
}

void assertLegacyCustomValueSaveCompatibility() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  LegacyCustomValue customValue;
  uint8_t loadBuffer[6];
  via::Config config = {1, 1, 1, keymap, defaults};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &customValue);
  assert(keyboard.begin(0));
  storage.reset();

  uint8_t packet[via::kPacketSize] = {};
  packet[1] = 2;
  for (uint8_t command = 0x07; command <= 0x09; ++command) {
    packet[0] = command;
    assert(keyboard.process(packet, 0));
  }
  assert(customValue.setCalls == 1);
  assert(customValue.getCalls == 1);
  assert(storage.commits == 1);

  packet[0] = 0x09;
  packet[1] = 7;
  assert(!keyboard.process(packet, 0));
  assert(packet[0] == 0xFF);
  assert(storage.commits == 1);
}

void assertOversizedCustomStateRejectedBeforeStorageAccess() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  OversizedCustomValue customValue;
  via::Config config = {1, 1, 1, keymap, defaults};
  via::Protocol keyboard(config, transport, &storage, &customValue);
  assert(!keyboard.begin(0));
  assert(storage.accesses == 0);
}

void assertOversizedCustomStateRejectedByDirectPersistence() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  OversizedCustomValue customValue;
  via::Config config = {1, 1, 1, keymap, defaults};
  via::Protocol keyboard(config, transport, &storage, &customValue);

  assert(!keyboard.save());
  assert(storage.accesses == 0);
  assert(storage.reads == 0);
  assert(storage.writes == 0);
  assert(storage.commits == 0);

  storage.reset();
  assert(!keyboard.load());
  assert(storage.accesses == 0);
  assert(storage.reads == 0);
  assert(storage.writes == 0);
  assert(storage.commits == 0);
}

void assertOversizedPayloadRejectedBeforeStorageAccess() {
  uint16_t keymap[255 * 255] = {};
  const uint16_t defaults[255 * 255] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  via::Config config = {255, 255, 1, keymap, defaults};
  via::Protocol keyboard(config, transport, &storage);

  assert(!keyboard.begin(0));
  assert(storage.accesses == 0);
}

void assertOversizedRecordRejectedBeforeStorageAccess() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  uint8_t macros[65520] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  via::Config config = {1, 1, 1, keymap, defaults, macros, sizeof(macros)};
  via::Protocol keyboard(config, transport, &storage);

  assert(!keyboard.begin(0));
  assert(storage.accesses == 0);
  assert(!keyboard.load());
  assert(storage.accesses == 0);
  assert(!keyboard.save());
  assert(storage.accesses == 0);
}

void assertLoadBufferAliasesRejectedBeforeStorageAccess() {
  uint16_t keymap[1];
  const uint16_t defaults[1] = {0x1001};
  uint16_t encoderMap[2];
  const uint16_t defaultEncoderMap[2] = {0x2001, 0x2002};
  uint8_t macros[4];
  via::MemoryTransport transport;
  RecordingStorage storage;
  uint8_t* aliases[] = {reinterpret_cast<uint8_t*>(keymap),
                        reinterpret_cast<uint8_t*>(encoderMap), macros};

  for (uint8_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
    keymap[0] = 0xA001;
    encoderMap[0] = 0xB001;
    encoderMap[1] = 0xB002;
    memset(macros, 0xC1, sizeof(macros));
    via::Config config = {1, 1, 1, keymap, defaults, macros, sizeof(macros),
                          1, 0, 0, 0, 1, encoderMap, defaultEncoderMap};
    config.loadBuffer = aliases[i];
    config.loadBufferBytes = 14;
    via::Protocol keyboard(config, transport, &storage);

    assert(!keyboard.begin(0));
    assert(storage.accesses == 0);
    assert(keymap[0] == 0xA001);
    assert(encoderMap[0] == 0xB001 && encoderMap[1] == 0xB002);
    assert(macros[0] == 0xC1 && macros[3] == 0xC1);

    storage.reset();
    assert(!keyboard.load());
    assert(storage.accesses == 0);
    assert(keymap[0] == 0xA001);
    assert(encoderMap[0] == 0xB001 && encoderMap[1] == 0xB002);
    assert(macros[0] == 0xC1 && macros[3] == 0xC1);
  }
}

void assertRequiredLoadBufferSizeAndMigrationGuards() {
  uint16_t keymap[2] = {};
  const uint16_t defaults[2] = {};
  uint16_t encoderMap[2] = {};
  const uint16_t defaultEncoderMap[2] = {};
  uint8_t macros[3] = {};
  via::MemoryTransport transport;
  RecordingStorage storage;
  StoredCustomValue customValue(0);
  via::Config config = {1, 2, 1, keymap, defaults, macros, sizeof(macros),
                        1, 0, 0, 0, 1, encoderMap, defaultEncoderMap};
  via::Protocol keyboard(config, transport, &storage, &customValue);

  assert(keyboard.requiredLoadBufferSize() == 16);
  assert(!keyboard.begin(0));
  assert(storage.accesses == 0);
  assert(!keyboard.load());
  assert(storage.accesses == 0);

  uint8_t undersized[15];
  config.loadBuffer = undersized;
  config.loadBufferBytes = sizeof(undersized);
  via::Protocol undersizedKeyboard(config, transport, &storage, &customValue);
  assert(!undersizedKeyboard.begin(0));
  assert(storage.accesses == 0);
  assert(!undersizedKeyboard.load());
  assert(storage.accesses == 0);
}

void assertTenFieldConfigWithoutStorageBegins() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  uint8_t macros[1] = {};
  via::MemoryTransport transport;
  via::Config config = {1, 1, 1, keymap, defaults, macros, sizeof(macros),
                        1, 0x01020304UL, 100};
  via::Protocol keyboard(config, transport);

  assert(keyboard.begin(0));
}

void assertCorruptLoadDoesNotMutateActiveState() {
  uint16_t keymap[2] = {0x1001, 0x1002};
  const uint16_t defaults[2] = {0x1001, 0x1002};
  uint16_t encoderMap[2] = {0x2001, 0x2002};
  const uint16_t defaultEncoderMap[2] = {0x2001, 0x2002};
  uint8_t macros[2] = {0x31, 0x32};
  uint8_t storageBytes[64] = {};
  uint8_t loadBuffer[15];
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  StoredCustomValue customValue(0x41);
  via::Config config = {1, 2, 1, keymap, defaults, macros, sizeof(macros), 1, 0, 0,
                        0x30010203UL, 1, encoderMap, defaultEncoderMap};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &customValue);
  assert(keyboard.begin(0));
  assert(keyboard.save());

  keymap[0] = 0xA001;
  keymap[1] = 0xA002;
  encoderMap[0] = 0xB001;
  encoderMap[1] = 0xB002;
  macros[0] = 0xC1;
  macros[1] = 0xC2;
  customValue.value = 0xD1;
  uint8_t packet[via::kPacketSize] = {};
  packet[0] = 0x03;
  packet[1] = 0x02;
  packet[2] = 0xE0;
  packet[3] = 0x01;
  packet[4] = 0x02;
  packet[5] = 0x03;
  assert(keyboard.process(packet, 0));
  const uint8_t corruptOffsets[] = {12, 16, 20, 22, 26};
  for (uint8_t i = 0; i < sizeof(corruptOffsets); ++i) {
    storageBytes[corruptOffsets[i]] ^= 0x01;
    assert(!keyboard.load());
    assert(keymap[0] == 0xA001 && keymap[1] == 0xA002);
    assert(encoderMap[0] == 0xB001 && encoderMap[1] == 0xB002);
    assert(macros[0] == 0xC1 && macros[1] == 0xC2);
    assert(keyboard.layoutOptions() == 0xE0010203UL);
    assert(customValue.value == 0xD1);
    assert(customValue.loadCalls == 0);
    storageBytes[corruptOffsets[i]] ^= 0x01;
  }
}

void assertSecondPassFailureDoesNotMutateActiveState() {
  uint16_t keymap[2] = {0x1001, 0x1002};
  const uint16_t defaults[2] = {0x1001, 0x1002};
  uint16_t encoderMap[2] = {0x2001, 0x2002};
  const uint16_t defaultEncoderMap[2] = {0x2001, 0x2002};
  uint8_t macros[2] = {};
  uint8_t storageBytes[64];
  uint8_t loadBuffer[15];
  via::MemoryTransport transport;
  FailingReadStorage storage(storageBytes, sizeof(storageBytes));
  StoredCustomValue customValue(0x41);
  via::Config config = {1, 2, 1, keymap, defaults, macros, sizeof(macros), 1, 0, 0,
                        0x30010203UL, 1, encoderMap, defaultEncoderMap};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &customValue);
  assert(keyboard.begin(0));
  assert(keyboard.save());

  keymap[0] = 0xA001;
  keymap[1] = 0xA002;
  encoderMap[0] = 0xB001;
  encoderMap[1] = 0xB002;
  macros[0] = 0xC1;
  macros[1] = 0xC2;
  customValue.value = 0xD1;
  uint8_t packet[via::kPacketSize] = {0x03, 0x02, 0xE0, 0x01, 0x02, 0x03};
  assert(keyboard.process(packet, 0));
  storage.failRead(4);

  assert(!keyboard.load());
  assert(keymap[0] == 0xA001 && keymap[1] == 0xA002);
  assert(encoderMap[0] == 0xB001 && encoderMap[1] == 0xB002);
  assert(macros[0] == 0xC1 && macros[1] == 0xC2);
  assert(keyboard.layoutOptions() == 0xE0010203UL);
  assert(customValue.value == 0xD1);
  assert(customValue.loadCalls == 0);
}

void assertCustomLoadRejectionDoesNotMutateActiveState() {
  uint16_t keymap[1] = {0x1001};
  const uint16_t defaults[1] = {0x1001};
  uint8_t macros[1] = {};
  uint8_t storageBytes[32] = {};
  uint8_t loadBuffer[8];
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  StoredCustomValue saved(0x41);
  via::Config config = {1, 1, 1, keymap, defaults, macros, sizeof(macros), 0, 0, 0,
                        0x30010203UL};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &saved);
  assert(keyboard.begin(0));
  assert(keyboard.save());

  keymap[0] = 0xA001;
  macros[0] = 0xC1;
  uint8_t packet[via::kPacketSize] = {0x03, 0x02, 0xE0, 0x01, 0x02, 0x03};
  assert(keyboard.process(packet, 0));
  StoredCustomValue rejecting(0xD1, false, true);
  via::Protocol rejectingKeyboard(config, transport, &storage, &rejecting);
  packet[2] = 0xE0;
  assert(rejectingKeyboard.process(packet, 0));

  assert(!rejectingKeyboard.load());
  assert(keymap[0] == 0xA001);
  assert(macros[0] == 0xC1);
  assert(rejectingKeyboard.layoutOptions() == 0xE0010203UL);
  assert(rejecting.value == 0xD1);
  assert(rejecting.loadCalls == 1);
}

void assertBulkBoundsAndEmptyWrites() {
  uint16_t keymap[1] = {0x1234};
  const uint16_t defaults[1] = {0x1234};
  uint8_t macros[2] = {0x56, 0x78};
  via::MemoryTransport transport;
  RecordingCallbacks callbacks;
  via::Config config = {1, 1, 1, keymap, defaults, macros, sizeof(macros)};
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));
  macros[0] = 0x56;
  macros[1] = 0x78;

  const uint8_t getCommands[] = {0x0E, 0x12};
  uint8_t packet[via::kPacketSize];
  for (uint8_t i = 0; i < sizeof(getCommands); ++i) {
    memset(packet, 0xA5, sizeof(packet));
    packet[0] = getCommands[i];
    packet[1] = 0xFF;
    packet[2] = 0xFF;
    packet[3] = 2;
    assert(keyboard.process(packet, 0));
    assert(packet[4] == 0 && packet[5] == 0);
  }

  const uint8_t setCommands[] = {0x0F, 0x13};
  for (uint8_t i = 0; i < sizeof(setCommands); ++i) {
    memset(packet, 0xA5, sizeof(packet));
    packet[0] = setCommands[i];
    packet[1] = 0xFF;
    packet[2] = 0xFF;
    packet[3] = 2;
    assert(keyboard.process(packet, 0));
  }
  assert(keymap[0] == 0x1234);
  assert(macros[0] == 0x56 && macros[1] == 0x78);
  assert(!keyboard.dirty());
  assert(callbacks.changeCalls == 0);

  for (uint8_t i = 0; i < sizeof(setCommands); ++i) {
    memset(packet, 0, sizeof(packet));
    packet[0] = setCommands[i];
    packet[3] = 0;
    assert(keyboard.process(packet, 0));
  }
  assert(!keyboard.dirty());
  assert(callbacks.changeCalls == 0);
}

void assertCustomStateSerializedOncePerSave() {
  uint16_t keymap[1] = {0x1234};
  const uint16_t defaults[1] = {0x1234};
  uint8_t storageBytes[32] = {};
  uint8_t loadBuffer[7];
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  StoredCustomValue saved(0, true);
  via::Config config = {1, 1, 1, keymap, defaults};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, &saved);
  assert(keyboard.begin(0));
  assert(keyboard.save());
  assert(saved.saveCalls == 1);

  uint16_t restoredKeymap[1] = {};
  via::MemoryTransport restoredTransport;
  StoredCustomValue restored(0);
  via::Config restoredConfig = {1, 1, 1, restoredKeymap, defaults};
  restoredConfig.loadBuffer = loadBuffer;
  restoredConfig.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol restoredKeyboard(restoredConfig, restoredTransport, &storage, &restored);
  assert(restoredKeyboard.begin(0));
  assert(restored.value == 1);
  assert(restored.loadCalls == 1);
}

void assertRGBLightSaveChannel() {
  via::RGBLightState state = {};
  via::RGBLight light(state);
  uint8_t packet[via::kPacketSize] = {};
  packet[1] = 2;
  assert(light.save(packet));
  packet[1] = 7;
  assert(!light.save(packet));
}

void assertSensitiveCommandsAreOptIn() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  uint8_t loadBuffer[6];
  RecordingStorage storage;
  via::MemoryTransport transport;
  SecurityCallbacks callbacks;
  via::Config config = {1, 1, 1, keymap, defaults};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
  via::Protocol keyboard(config, transport, &storage, nullptr, &callbacks);
  assert(keyboard.begin(0));
  storage.reset();

  uint8_t packet[via::kPacketSize];
  memset(packet, 0xA5, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x03;
  packet[2] = 0;
  assert(keyboard.process(packet, 0));
  for (uint8_t i = 3; i < via::kPacketSize; ++i) assert(packet[i] == 0);
  assert(callbacks.matrixCalls == 0);

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x0A;
  assert(!keyboard.process(packet, 0));
  assert(packet[0] == 0xFF);
  assert(storage.accesses == 0);

  packet[0] = 0x0B;
  assert(!keyboard.process(packet, 0));
  assert(packet[0] == 0xFF);
  assert(callbacks.bootloaderCalls == 0);

  config.matrixStateEnabled = true;
  via::Protocol matrixKeyboard(config, transport, &storage, nullptr, &callbacks);
  assert(matrixKeyboard.begin(0));
  storage.reset();
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x03;
  assert(matrixKeyboard.process(packet, 0));
  assert(packet[3] == 0xA5);
  assert(callbacks.matrixCalls == 1);
  packet[0] = 0x0A;
  assert(!matrixKeyboard.process(packet, 0));
  packet[0] = 0x0B;
  assert(!matrixKeyboard.process(packet, 0));
  assert(storage.accesses == 0);
  assert(callbacks.bootloaderCalls == 0);

  config.matrixStateEnabled = false;
  config.eepromResetEnabled = true;
  via::Protocol resetKeyboard(config, transport, &storage, nullptr, &callbacks);
  assert(resetKeyboard.begin(0));
  storage.reset();
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x03;
  assert(resetKeyboard.process(packet, 0));
  assert(packet[3] == 0);
  assert(callbacks.matrixCalls == 1);
  packet[0] = 0x0A;
  assert(resetKeyboard.process(packet, 0));
  assert(storage.erases == 1 && storage.commits == 1);
  packet[0] = 0x0B;
  assert(!resetKeyboard.process(packet, 0));
  assert(callbacks.bootloaderCalls == 0);

  config.eepromResetEnabled = false;
  config.bootloaderEnabled = true;
  via::Protocol bootloaderKeyboard(config, transport, &storage, nullptr, &callbacks);
  assert(bootloaderKeyboard.begin(0));
  storage.reset();
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x02;
  packet[1] = 0x03;
  assert(bootloaderKeyboard.process(packet, 0));
  assert(packet[3] == 0);
  packet[0] = 0x0A;
  assert(!bootloaderKeyboard.process(packet, 0));
  packet[0] = 0x0B;
  assert(bootloaderKeyboard.process(packet, 0));
  assert(storage.accesses == 0);
  assert(callbacks.matrixCalls == 1);
  assert(callbacks.bootloaderCalls == 0);
}

void assertFailedSendRetriesWithoutReprocessing() {
  uint16_t keymap[1] = {0x0004};
  const uint16_t defaults[1] = {0x0004};
  FailFirstTransport transport;
  RecordingCallbacks callbacks;
  via::Config config = {1, 1, 1, keymap, defaults};
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));

  const uint8_t first[via::kPacketSize] = {0x05, 0, 0, 0, 0x12, 0x34};
  const uint8_t second[via::kPacketSize] = {0x05, 0, 0, 0, 0x56, 0x78};
  transport.queue(first);
  transport.queue(second);

  keyboard.task(1);
  assert(transport.receiveCalls == 1 && transport.delivered == 1);
  assert(transport.sendCalls == 1);
  assert(keymap[0] == 0x1234 && callbacks.changeCalls == 1);

  keyboard.task(2);
  assert(transport.receiveCalls == 1 && transport.delivered == 1);
  assert(transport.sendCalls == 2);
  assert(memcmp(transport.sent[0], transport.sent[1], via::kPacketSize) == 0);
  assert(keymap[0] == 0x1234 && callbacks.changeCalls == 1);

  keyboard.task(3);
  assert(transport.receiveCalls == 2 && transport.delivered == 2);
  assert(transport.sendCalls == 3);
  assert(keymap[0] == 0x5678 && callbacks.changeCalls == 2);
}

void assertBootloaderWaitsForSuccessfulSend() {
  uint16_t keymap[1] = {};
  const uint16_t defaults[1] = {};
  FailFirstTransport transport;
  SecurityCallbacks callbacks;
  via::Config config = {1, 1, 1, keymap, defaults};
  config.bootloaderEnabled = true;
  via::Protocol keyboard(config, transport, nullptr, nullptr, &callbacks);
  assert(keyboard.begin(0));

  const uint8_t request[via::kPacketSize] = {0x0B};
  transport.queue(request);
  keyboard.task(1);
  assert(transport.sendCalls == 1);
  assert(callbacks.bootloaderCalls == 0);

  keyboard.task(2);
  assert(transport.receiveCalls == 1);
  assert(transport.sendCalls == 2);
  assert(callbacks.bootloaderCalls == 1);

  keyboard.task(3);
  assert(callbacks.bootloaderCalls == 1);
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
  assertLegacyIndicationCallback();
  assertCustomValueRouting();
  assertLegacyCustomValueSaveCompatibility();
  assertOversizedCustomStateRejectedBeforeStorageAccess();
  assertOversizedCustomStateRejectedByDirectPersistence();
  assertOversizedPayloadRejectedBeforeStorageAccess();
  assertCorruptLoadDoesNotMutateActiveState();
  assertOversizedRecordRejectedBeforeStorageAccess();
  assertLoadBufferAliasesRejectedBeforeStorageAccess();
  assertRequiredLoadBufferSizeAndMigrationGuards();
  assertTenFieldConfigWithoutStorageBegins();
  assertCustomLoadRejectionDoesNotMutateActiveState();
  assertSecondPassFailureDoesNotMutateActiveState();
  assertBulkBoundsAndEmptyWrites();
  assertCustomStateSerializedOncePerSave();
  assertRGBLightSaveChannel();
  assertSensitiveCommandsAreOptIn();
  assertFailedSendRetriesWithoutReprocessing();
  assertBootloaderWaitsForSuccessfulSend();

  uint16_t keymap[4] = {0x0004, 0x0005, 0x0014, 0x001A};
  const uint16_t defaults[4] = {0x0004, 0x0005, 0x0014, 0x001A};
  uint16_t encoderMap[4] = {};
  const uint16_t defaultEncoderMap[4] = {0x0101, 0x0102, 0x0201, 0x0202};
  uint8_t macros[64] = {};
  uint8_t storageBytes[128] = {};
  uint8_t loadBuffer[84];
  via::MemoryTransport transport;
  via::MemoryStorage storage(storageBytes, sizeof(storageBytes));
  RecordingCallbacks callbacks;
  via::Config config = {1, 2, 2, keymap, defaults, macros, sizeof(macros), 2, 1, 1,
                        0x01020304UL, 1, encoderMap, defaultEncoderMap};
  config.loadBuffer = loadBuffer;
  config.loadBufferBytes = sizeof(loadBuffer);
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
  assertEncoderRejected(keyboard, 0x15, 2, 0, 0);
  assertEncoderRejected(keyboard, 0x15, 0, 1, 0);
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
  restoredConfig.loadBuffer = loadBuffer;
  restoredConfig.loadBufferBytes = sizeof(loadBuffer);
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
