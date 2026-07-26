#include <assert.h>
#include <string.h>

#include "VIA_Arduino.h"

int main() {
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
  return 0;
}
