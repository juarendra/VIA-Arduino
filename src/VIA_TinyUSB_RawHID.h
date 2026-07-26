#pragma once

#include "VIA_Protocol.h"

#if __has_include(<Adafruit_TinyUSB.h>)

#include <Adafruit_TinyUSB.h>

namespace via {
namespace tinyusb {

/* Adafruit TinyUSB Raw HID adapter. The descriptor is deliberately its own
 * vendor-defined HID interface: VIA must not share a report ID with keyboard
 * reports. One RawHID instance is supported per USB device. */
class RawHID : public Transport {
 public:
  RawHID();
  bool begin(const char* interfaceName = "VIA Raw HID");
  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool ready() const;

 private:
  static void setReport(uint8_t reportId, hid_report_type_t reportType,
                        uint8_t const* buffer, uint16_t length);
  void receiveReport(uint8_t reportId, hid_report_type_t reportType,
                     uint8_t const* buffer, uint16_t length);

  Adafruit_USBD_HID hid_;
  uint8_t rx_[kPacketSize];
  volatile bool rxReady_;
  static RawHID* active_;
};

}  // namespace tinyusb
}  // namespace via

#else

#error "VIA_TinyUSB_RawHID requires Adafruit TinyUSB Library. Install it and select a TinyUSB USB stack."

#endif
