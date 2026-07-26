#include "VIA_TinyUSB_RawHID.h"

#if __has_include(<Adafruit_TinyUSB.h>)

#include <string.h>

namespace via {
namespace tinyusb {
namespace {

/* QMK/VIA Raw HID: vendor page FF60, usage 61, 32-byte IN and OUT reports,
 * no Report ID. Keep this as a dedicated HID interface. */
uint8_t const kViaRawHidDescriptor[] = {
    0x06, 0x60, 0xFF,  // Usage Page (Vendor Defined 0xFF60)
    0x09, 0x61,        // Usage (0x61)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, kPacketSize,
    0x09, 0x62,
    0x81, 0x02,        // Input (Data,Var,Abs)
    0x95, kPacketSize,
    0x09, 0x63,
    0x91, 0x02,        // Output (Data,Var,Abs)
    0xC0,
};

}  // namespace

RawHID* RawHID::active_ = nullptr;

RawHID::RawHID()
    : hid_(kViaRawHidDescriptor, sizeof(kViaRawHidDescriptor),
           HID_ITF_PROTOCOL_NONE, 2, true),
      rxReady_(false) {
  memset(rx_, 0, sizeof(rx_));
}

bool RawHID::begin(const char* interfaceName) {
  if (active_ && active_ != this) return false;
  active_ = this;
  if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
  hid_.enableOutEndpoint(true);
  hid_.setPollInterval(2);
  hid_.setStringDescriptor(interfaceName);
  hid_.setReportCallback(nullptr, setReport);
  return hid_.begin();
}

bool RawHID::receive(uint8_t packet[kPacketSize]) {
  if (!rxReady_) return false;
  noInterrupts();
  memcpy(packet, rx_, kPacketSize);
  rxReady_ = false;
  interrupts();
  return true;
}

bool RawHID::send(const uint8_t packet[kPacketSize]) {
  return hid_.ready() && hid_.sendReport(0, packet, kPacketSize);
}

bool RawHID::ready() { return hid_.ready(); }

void RawHID::setReport(uint8_t reportId, hid_report_type_t reportType,
                       uint8_t const* buffer, uint16_t length) {
  if (active_) active_->receiveReport(reportId, reportType, buffer, length);
}

void RawHID::receiveReport(uint8_t reportId, hid_report_type_t reportType,
                           uint8_t const* buffer, uint16_t length) {
  if (reportId != 0 || reportType != HID_REPORT_TYPE_OUTPUT || length != kPacketSize) return;
  memcpy(rx_, buffer, kPacketSize);
  rxReady_ = true;
}

}  // namespace tinyusb
}  // namespace via

#endif
