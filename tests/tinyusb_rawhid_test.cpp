#include <assert.h>
#include <string.h>

#include "VIA_TinyUSB_RawHID.h"

bool Adafruit_USBD_HID::beginResult = true;
bool Adafruit_USBD_HID::readyResult = true;
bool Adafruit_USBD_HID::sendResult = true;
size_t Adafruit_USBD_HID::beginAttempts = 0;
Adafruit_USBD_HID* Adafruit_USBD_HID::registeredInterface = 0;
TinyUSBDeviceClass TinyUSBDevice;

static void assertInactive(via::tinyusb::RawHID& hid) {
  uint8_t packet[via::kPacketSize] = {};
  assert(!hid.receive(packet));
  assert(!hid.send(packet));
  assert(!hid.ready());
  assert(!hid.sendComplete());
}

static void assertFailedBeginIsFinal() {
  Adafruit_USBD_HID::beginResult = false;
  via::tinyusb::RawHID failed;
  assert(!failed.begin());
  assert(Adafruit_USBD_HID::beginAttempts == 1);
  assert(Adafruit_USBD_HID::registeredInterface == 0);
  assertInactive(failed);

  Adafruit_USBD_HID::beginResult = true;
  via::tinyusb::RawHID replacement;
  assert(!replacement.begin());
  assert(Adafruit_USBD_HID::beginAttempts == 1);
  assertInactive(replacement);
}

static void assertSuccessfulOwnerIsFinal() {
  uint8_t first[via::kPacketSize] = {0x41};
  uint8_t second[via::kPacketSize] = {0x42};

  {
    via::tinyusb::RawHID owner;
    assert(owner.begin());
    assert(Adafruit_USBD_HID::beginAttempts == 1);

    via::tinyusb::RawHID rejected;
    assert(!rejected.begin());
    assert(Adafruit_USBD_HID::beginAttempts == 1);
    assertInactive(rejected);

    Adafruit_USBD_HID::dispatchSetReport(0, HID_REPORT_TYPE_OUTPUT, first,
                                        sizeof(first));
    Adafruit_USBD_HID::dispatchSetReport(0, HID_REPORT_TYPE_OUTPUT, second,
                                        sizeof(second));
    uint8_t received[via::kPacketSize] = {};
    assert(owner.receive(received));
    assert(memcmp(received, first, sizeof(first)) == 0);
    assert(!owner.receive(received));

    assert(owner.send(first));
    Adafruit_USBD_HID::readyResult = false;
    assert(!owner.send(first));
    assert(!owner.ready());
    assert(!owner.sendComplete());
    Adafruit_USBD_HID::readyResult = true;
    assert(owner.ready());
    assert(owner.sendComplete());
  }

  Adafruit_USBD_HID::dispatchSetReport(0, HID_REPORT_TYPE_OUTPUT, first,
                                      sizeof(first));
  via::tinyusb::RawHID replacement;
  assert(!replacement.begin());
  assert(Adafruit_USBD_HID::beginAttempts == 1);
  assertInactive(replacement);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  if (strcmp(argv[1], "failed") == 0) {
    assertFailedBeginIsFinal();
  } else {
    assert(strcmp(argv[1], "successful") == 0);
    assertSuccessfulOwnerIsFinal();
  }
}
