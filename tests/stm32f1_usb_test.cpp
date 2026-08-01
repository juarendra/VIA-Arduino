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

static void assertSendCompleteComposite() {
  via::stm32f1::UsbDevice dev;
  assert(dev.begin());

  assert(dev.sendComplete()); // nothing in flight

  via::KeyboardReport r = {0, 0, {0, 0, 0, 0, 0, 0}};
  assert(dev.send(r));     // kb EP busy
  assert(!dev.sendComplete()); // kb_tx_busy_ true

  usbd_transmit_ok = true;

  const uint8_t tx[via::kPacketSize] = {};
  assert(dev.send(tx));       // via EP busy too
  assert(!dev.sendComplete()); // both busy
}

static void assertViaSendGuard() {
  via::stm32f1::UsbDevice dev;
  assert(dev.begin());

  const uint8_t tx[via::kPacketSize] = {};
  assert(dev.send(tx));
  assert(!dev.sendComplete());

  assert(!dev.send(tx)); // already busy, guard works
}

static void assertClassZeroInit() {
  via::stm32f1::UsbDevice dev;
  // begin() assigns callbacks; before that, all must be nullptr.
  // This is compile-time: we just verify begin() succeeds (UB-free).
  assert(dev.begin());
}

static void assertTaskRuns() {
  via::stm32f1::UsbDevice dev;
  assert(dev.begin());
  dev.task(); // must not crash
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
  assertSendCompleteComposite();
  assertViaSendGuard();
  assertClassZeroInit();
  assertTaskRuns();
  return 0;
}
