#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t hid_report_type_t;

static const uint8_t HID_ITF_PROTOCOL_NONE = 0;
static const hid_report_type_t HID_REPORT_TYPE_OUTPUT = 2;

class Adafruit_USBD_HID {
 public:
  typedef void (*SetReportCallback)(uint8_t, hid_report_type_t, const uint8_t*,
                                    uint16_t);

  Adafruit_USBD_HID(const uint8_t*, size_t, uint8_t, uint8_t, bool)
      : setReportCallback_(0), alive_(true) {}
  ~Adafruit_USBD_HID() { alive_ = false; }

  void enableOutEndpoint(bool) {}
  void setPollInterval(uint8_t) {}
  void setStringDescriptor(const char*) {}
  void setReportCallback(void*, SetReportCallback callback) {
    setReportCallback_ = callback;
  }
  bool begin() {
    ++beginAttempts;
    if (registeredInterface) return false;
    registeredInterface = this;
    return beginResult;
  }
  bool ready() { return readyResult; }
  bool sendReport(uint8_t, const uint8_t*, size_t) { return sendResult; }

  static void dispatchSetReport(uint8_t reportId,
                                hid_report_type_t reportType,
                                const uint8_t* buffer, uint16_t length) {
    assert(registeredInterface);
    assert(registeredInterface->alive_);
    if (registeredInterface->setReportCallback_) {
      registeredInterface->setReportCallback_(reportId, reportType, buffer,
                                               length);
    }
  }

  static bool beginResult;
  static bool readyResult;
  static bool sendResult;
  static size_t beginAttempts;
  static Adafruit_USBD_HID* registeredInterface;

 private:
  SetReportCallback setReportCallback_;
  bool alive_;
};

class TinyUSBDeviceClass {
 public:
  bool isInitialized() const { return true; }
  void begin(uint8_t) {}
};

extern TinyUSBDeviceClass TinyUSBDevice;

inline void noInterrupts() {}
inline void interrupts() {}
