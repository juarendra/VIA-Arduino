#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint8_t hid_report_type_t;

static const uint8_t HID_ITF_PROTOCOL_NONE = 0;
static const hid_report_type_t HID_REPORT_TYPE_OUTPUT = 2;

class Adafruit_USBD_HID {
 public:
  Adafruit_USBD_HID(const uint8_t*, size_t, uint8_t, uint8_t, bool) {}

  void enableOutEndpoint(bool) {}
  void setPollInterval(uint8_t) {}
  void setStringDescriptor(const char*) {}
  void setReportCallback(
      void*, void (*)(uint8_t, hid_report_type_t, const uint8_t*, uint16_t)) {}
  bool begin() { return beginResult; }
  bool ready() { return true; }
  bool sendReport(uint8_t, const uint8_t*, size_t) { return true; }

  static bool beginResult;
};

class TinyUSBDeviceClass {
 public:
  bool isInitialized() const { return true; }
  void begin(uint8_t) {}
};

extern TinyUSBDeviceClass TinyUSBDevice;

inline void noInterrupts() {}
inline void interrupts() {}
