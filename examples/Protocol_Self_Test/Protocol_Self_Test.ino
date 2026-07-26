#include <VIA_Arduino.h>

/* This example verifies the portable protocol core without a USB backend.
 * For a real board, replace MemoryTransport with a Transport implementation
 * backed by a dedicated native-USB Raw HID interface. */

static uint16_t keymap[2 * 1 * 2] = {0x0004, 0x0005, 0x0014, 0x001A};
static const uint16_t defaultKeymap[2 * 1 * 2] = {0x0004, 0x0005, 0x0014, 0x001A};
static uint8_t macros[64];
static uint8_t savedSettings[128];

via::MemoryTransport transport;
via::MemoryStorage storage(savedSettings, sizeof(savedSettings));
via::Config config = {
    1, 2, 2, keymap, defaultKeymap, macros, sizeof(macros), 2, 1, 750,
};
via::Protocol keyboard(config, transport, &storage);

void setup() {
  Serial.begin(115200);
  keyboard.begin(millis());

  uint8_t request[via::kPacketSize] = {};
  request[0] = 0x01;  // VIA get protocol version
  transport.inject(request);
  keyboard.task(millis());

  uint8_t response[via::kPacketSize];
  if (transport.takeResponse(response) && response[1] == 0 && response[2] == via::kProtocolVersion) {
    Serial.println("VIA core is ready. Add a native USB Raw HID Transport next.");
  } else {
    Serial.println("VIA core self-test failed.");
  }
}

void loop() {
  keyboard.task(millis());
}
