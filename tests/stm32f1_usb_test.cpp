#define STM32F1XX

#include <assert.h>
#include <string.h>

#include "VIA_STM32F1_USB.h"

#include "usbd_core.h"

static void assertTwoBeginsFails() {
  via::stm32f1::UsbDevice a;
  assert(a.begin());
  assert(!a.configured());

  via::stm32f1::UsbDevice b;
  assert(!b.begin());
}

static void assertKeyboardHIDContract() {
  via::stm32f1::UsbDevice dev;
  assert(dev.begin());

  assert(!dev.configured());
  assert(!dev.suspended());
  assert(dev.remoteWakeupAllowed());
  assert(!dev.remoteWakeup());

  via::KeyboardReport r = {0x01, 0, {0x04, 0x05, 0, 0, 0, 0}};
  assert(dev.send(r));
  assert(!dev.sendComplete());

  assert(usbd_last_ep == 0x81);
  assert(usbd_last_len == 8);
  assert(usbd_last_buf[0] == 0x01);
  assert(usbd_last_buf[1] == 0x00);
  assert(usbd_last_buf[2] == 0x04);
  assert(usbd_last_buf[3] == 0x05);

  assert(!dev.send(r));

  usbd_transmit_ok = true;

  uint8_t leds = 0xFF;
  assert(!dev.takeHostLeds(leds));
  assert(leds == 0xFF);

  assert(!dev.suspended());
}

static void assertTransportContract() {
  via::stm32f1::UsbDevice dev;
  assert(dev.begin());

  assert(dev.sendComplete());

  const uint8_t tx[via::kPacketSize] = {};
  assert(dev.send(tx));
  assert(!dev.sendComplete());

  assert(usbd_last_ep == 0x82);
  assert(usbd_last_len == via::kPacketSize);

  uint8_t rx[via::kPacketSize] = {};
  assert(!dev.receive(rx));
}

static void assertBufferSizes() {
  assert(via::stm32f1::kKeyboardReportSize == 8);
  assert(via::stm32f1::kViaReportSize == via::kPacketSize);
}

static void assertEndpointConstants() {
  assert(via::stm32f1::kKeyboardEpIn == 0x81);
  assert(via::stm32f1::kViaEpIn == 0x82);
  assert(via::stm32f1::kViaEpOut == 0x02);
}

int main() {
  assertTwoBeginsFails();
  assertKeyboardHIDContract();
  assertTransportContract();
  assertBufferSizes();
  assertEndpointConstants();
  return 0;
}
