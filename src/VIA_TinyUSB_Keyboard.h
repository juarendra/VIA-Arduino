#pragma once

#if __has_include(<Adafruit_TinyUSB.h>)

#include <Adafruit_TinyUSB.h>

namespace via {
namespace tinyusb {

/* Optional companion interface for a TinyUSB VIA device. It is intentionally
 * independent of RawHID, so the vendor interface remains report-ID-free. */
class Keyboard {
 public:
  Keyboard();
  bool begin(const char* interfaceName = "VIA Keyboard");
  bool report(uint8_t modifiers, const uint8_t keys[6]);
  bool release();
  bool ready() const;

 private:
  Adafruit_USBD_HID hid_;
};

}  // namespace tinyusb
}  // namespace via

#else

#error "VIA_TinyUSB_Keyboard requires Adafruit TinyUSB Library."

#endif
