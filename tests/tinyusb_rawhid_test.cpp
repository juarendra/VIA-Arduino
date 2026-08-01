#include <assert.h>

#include "VIA_TinyUSB_RawHID.h"

bool Adafruit_USBD_HID::beginResult = true;
TinyUSBDeviceClass TinyUSBDevice;

int main() {
  {
    via::tinyusb::RawHID failed;
    Adafruit_USBD_HID::beginResult = false;
    assert(!failed.begin());

    via::tinyusb::RawHID replacement;
    Adafruit_USBD_HID::beginResult = true;
    assert(replacement.begin());
  }

  {
    via::tinyusb::RawHID owner;
    assert(owner.begin());
  }
  via::tinyusb::RawHID replacement;
  assert(replacement.begin());
}
