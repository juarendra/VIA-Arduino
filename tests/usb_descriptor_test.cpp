#include <assert.h>
#include <string.h>

#include "VIA_STM32F1_USBDescriptor.h"

static void assertDeviceDescriptor() {
  const uint8_t* d = via::stm32f1::kDeviceDescriptor;
  assert(d[0] == 18);
  assert(d[1] == 1);
  assert(d[8] == 0xFE);
  assert(d[9] == 0xCA);
  assert(d[10] == 0x03);
  assert(d[11] == 0x40);
  assert(d[12] == 0x01);
  assert(d[13] == 0x00);
}

static void assertConfigDescriptor() {
  const uint8_t* p = via::stm32f1::kConfigDescriptor;
  assert(p[0] == 9);
  assert(p[1] == 2);
  assert(p[4] == 2);
  assert(p[5] == 1);
  assert(p[7] == 0x80);

  uint16_t totalLen = p[2] | (p[3] << 8);
  assert(totalLen >= 9 + 9 + 9 + 7 + 9 + 9 + 7 + 7);

  int ifaceCount = 0, epCount = 0, hidCount = 0;
  int epMaxPacket[4] = {};
  uint8_t ifaceSubclass[2] = {};
  uint8_t ifaceProtocol[2] = {};
  uint16_t hidReportLen[2] = {};
  const uint8_t* end = p + totalLen;
  p += 9;

  while (p < end) {
    uint8_t len = p[0];
    uint8_t type = p[1];
    if (type == 4) {
      assert(ifaceCount < 2);
      uint8_t num = p[2];
      assert(num == ifaceCount);
      ifaceSubclass[ifaceCount] = p[6];
      ifaceProtocol[ifaceCount] = p[7];
      ++ifaceCount;
    } else if (type == 5) {
      ++epCount;
      uint8_t addr = p[2];
      uint16_t mps = p[4] | (p[5] << 8);
      if (addr == 0x81) epMaxPacket[0] = mps;
      if (addr == 0x82) epMaxPacket[1] = mps;
      if (addr == 0x02) epMaxPacket[2] = mps;
    } else if (type == 0x21) {
      assert(hidCount < 2);
      hidReportLen[hidCount] = p[7] | (p[8] << 8);
      ++hidCount;
    }
    p += len;
  }

  assert(ifaceCount == 2);
  assert(epCount == 3);
  assert(hidCount == 2);

  assert(hidReportLen[0] == sizeof(via::stm32f1::kBootKeyboardReportDescriptor));
  assert(hidReportLen[1] == sizeof(via::stm32f1::kViaRawHidReportDescriptor));

  assert(ifaceSubclass[0] == 1);
  assert(ifaceProtocol[0] == 1);
  assert(ifaceSubclass[1] == 0);
  assert(ifaceProtocol[1] == 0);

  assert(epMaxPacket[0] == 8);
  assert(epMaxPacket[1] == 32);
  assert(epMaxPacket[2] == 32);
}

static void assertBootKeyboardReportDescriptor() {
  const uint8_t* r = via::stm32f1::kBootKeyboardReportDescriptor;
  assert(r[0] == 0x05 && r[1] == 0x01); // Usage Page (Generic Desktop)
  assert(r[2] == 0x09 && r[3] == 0x06); // Usage (Keyboard)
  assert(r[4] == 0xA1 && r[5] == 0x01); // Collection (Application)

  // usage page 0x07 (Keyboard/Keypad) at some point
  bool foundKbdPage = false;
  const uint8_t* end = r + sizeof(via::stm32f1::kBootKeyboardReportDescriptor);
  for (const uint8_t* s = r; s < end - 1; ++s) {
    if (s[0] == 0x05 && s[1] == 0x07) { foundKbdPage = true; break; }
  }
  assert(foundKbdPage);

  // ends with End Collection
  assert(end[-1] == 0xC0);

  // 8-byte input: verify 8 modifier bits (Report Size 1, Report Count 8)
  bool found8ModBits = false;
  for (const uint8_t* s = r; s < end - 3; ++s) {
    if (s[0] == 0x95 && s[1] == 0x08 &&
        s[2] == 0x75 && s[3] == 0x01) {
      found8ModBits = true;
      break;
    }
  }
  assert(found8ModBits);

  // 6 key slots: Report Count 6, Report Size 8, Input Array
  bool found6Keys = false;
  for (const uint8_t* s = r; s < end - 6; ++s) {
    if (s[0] == 0x95 && s[1] == 0x06 &&
        s[2] == 0x75 && s[3] == 0x08) {
      found6Keys = true;
      break;
    }
  }
  assert(found6Keys);

  // LED output: Usage Page 0x08 (LEDs)
  bool foundLeds = false;
  for (const uint8_t* s = r; s < end - 1; ++s) {
    if (s[0] == 0x05 && s[1] == 0x08) { foundLeds = true; break; }
  }
  assert(foundLeds);

  // no report ID
  assert(r[0] != 0x85);
}

static void assertViaRawHidReportDescriptor() {
  const uint8_t* r = via::stm32f1::kViaRawHidReportDescriptor;
  assert(r[0] == 0x06 && r[1] == 0x60 && r[2] == 0xFF); // Usage Page 0xFF60
  assert(r[3] == 0x09 && r[4] == 0x61);                   // Usage 0x61
  assert(r[5] == 0xA1 && r[6] == 0x01);                   // Collection (Application)

  // no report ID
  assert(r[0] != 0x85);

  // ends with End Collection
  const uint8_t* end = r + sizeof(via::stm32f1::kViaRawHidReportDescriptor);
  assert(end[-1] == 0xC0);

  // has Input (0x81) and Output (0x91) items
  bool foundInput = false, foundOutput = false;
  for (const uint8_t* s = r; s < end; ++s) {
    if (*s == 0x81) foundInput = true;
    if (*s == 0x91) foundOutput = true;
  }
  assert(foundInput && foundOutput);

  // Report Count 32 (0x20) appears twice (IN + OUT)
  int count32 = 0;
  for (const uint8_t* s = r; s < end - 1; ++s) {
    if (s[0] == 0x95 && s[1] == 0x20) ++count32;
  }
  assert(count32 == 2);
}

int main() {
  assertDeviceDescriptor();
  assertConfigDescriptor();
  assertBootKeyboardReportDescriptor();
  assertViaRawHidReportDescriptor();
  return 0;
}
