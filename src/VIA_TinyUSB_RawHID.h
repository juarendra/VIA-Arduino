#pragma once

#include "VIA_Protocol.h"

#if __has_include(<Adafruit_TinyUSB.h>)

#define VIA_ARDUINO_HAS_TINYUSB 1

#include <Adafruit_TinyUSB.h>

namespace via {
namespace tinyusb {

/* Adafruit TinyUSB Raw HID adapter. The descriptor is deliberately its own
 * vendor-defined HID interface: VIA must not share a report ID with keyboard
 * reports. Static interface state permits exactly one begin attempt per device
 * reset. */
class RawHID : public Transport {
 public:
  RawHID();
  ~RawHID();
  bool begin(const char* interfaceName = "VIA Raw HID");
  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override;
  bool ready();

 private:
  static void setReport(uint8_t reportId, hid_report_type_t reportType,
                        uint8_t const* buffer, uint16_t length);

  static Adafruit_USBD_HID hid_;
  static uint8_t rx_[kPacketSize];
  static volatile bool rxReady_;
  static RawHID* active_;
  static bool beginAttempted_;
};

}  // namespace tinyusb
}  // namespace via

#else

#define VIA_ARDUINO_HAS_TINYUSB 0

#endif  // __has_include(<Adafruit_TinyUSB.h>)
