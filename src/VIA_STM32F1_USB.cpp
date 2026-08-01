#include "VIA_STM32F1_USB.h"

#if defined(ARDUINO_ARCH_STM32) || defined(STM32F1xx)

#include <string.h>

extern "C" {
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint16_t);
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint16_t);
}

namespace via {
namespace stm32f1 {

UsbDevice* UsbDevice::active_ = nullptr;

UsbDevice::UsbDevice()
    : configured_(false), suspended_(false),
      kb_tx_busy_(false), via_tx_busy_(false),
      via_rx_ready_(false), led_ready_(false) {
  memset(&class_, 0, sizeof(class_));
  memset(kb_buf_, 0, sizeof(kb_buf_));
  memset(via_in_buf_, 0, sizeof(via_in_buf_));
  memset(via_out_buf_, 0, sizeof(via_out_buf_));
  led_buf_[0] = 0;
}

UsbDevice::~UsbDevice() {
  if (active_ == this) active_ = nullptr;
}

int8_t UsbDevice::classInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx) {
  (void)cfgidx;
  if (!active_) return USBD_FAIL;

  USBD_LL_PrepareReceive(pdev, kViaEpOut, active_->via_out_buf_, kViaReportSize);

  active_->configured_ = true;
  active_->suspended_ = false;
  return USBD_OK;
}

int8_t UsbDevice::classDeInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx) {
  (void)pdev;
  (void)cfgidx;
  if (!active_) return USBD_FAIL;
  active_->configured_ = false;
  return USBD_OK;
}

int8_t UsbDevice::classSetup(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req) {
  (void)pdev;
  if (!active_) return USBD_FAIL;

  if ((req->bmRequest & 0x7F) == 0x21 && req->bRequest == 0x09) {
    if (req->wValue == 0x0200 && req->wLength == 1) {
      USBD_CtlReceiveData(pdev, active_->led_buf_, 1);
      return USBD_OK;
    }
  }
  if ((req->bmRequest & 0x7F) == 0x21 && req->bRequest == 0x0A) {
    USBD_CtlSendData(pdev, 0, 0);
    return USBD_OK;
  }
  if ((req->bmRequest & 0x7F) == 0x21 && req->bRequest == 0x0B) {
    uint8_t proto = 1;
    USBD_CtlSendData(pdev, &proto, 1);
    return USBD_OK;
  }

  return USBD_OK;
}

int8_t UsbDevice::ep0TxSent(USBD_HandleTypeDef* pdev) {
  (void)pdev;
  return USBD_OK;
}

int8_t UsbDevice::ep0RxReady(USBD_HandleTypeDef* pdev) {
  (void)pdev;
  if (!active_) return USBD_FAIL;
  active_->led_ready_ = true;
  return USBD_OK;
}

int8_t UsbDevice::dataIn(USBD_HandleTypeDef* pdev, uint8_t epnum) {
  (void)pdev;
  if (!active_) return USBD_FAIL;
  if (epnum == (kKeyboardEpIn & 0x7F)) {
    active_->kb_tx_busy_ = false;
  } else if (epnum == (kViaEpIn & 0x7F)) {
    active_->via_tx_busy_ = false;
  }
  return USBD_OK;
}

int8_t UsbDevice::dataOut(USBD_HandleTypeDef* pdev, uint8_t epnum) {
  (void)pdev;
  if (!active_) return USBD_FAIL;
  if (epnum == (kViaEpOut & 0x7F)) {
    active_->via_rx_ready_ = true;
  }
  USBD_LL_PrepareReceive(pdev, kViaEpOut, active_->via_out_buf_, kViaReportSize);
  return USBD_OK;
}

int8_t UsbDevice::sof(USBD_HandleTypeDef* pdev) {
  (void)pdev;
  return USBD_OK;
}

int8_t UsbDevice::isoInIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum) {
  (void)pdev;
  (void)epnum;
  return USBD_OK;
}

int8_t UsbDevice::isoOutIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum) {
  (void)pdev;
  (void)epnum;
  return USBD_OK;
}

bool UsbDevice::begin() {
  if (active_) return false;
  active_ = this;

  class_.Init = classInit;
  class_.DeInit = classDeInit;
  class_.Setup = classSetup;
  class_.EP0_TxSent = ep0TxSent;
  class_.EP0_RxReady = ep0RxReady;
  class_.DataIn = dataIn;
  class_.DataOut = dataOut;
  class_.SOF = sof;
  class_.IsoINIncomplete = isoInIncomplete;
  class_.IsoOUTIncomplete = isoOutIncomplete;

  if (USBD_Init(&hUsbDevice_, &class_, 0) != USBD_OK) {
    if (active_ == this) active_ = nullptr;
    return false;
  }
  if (USBD_Start(&hUsbDevice_) != USBD_OK) {
    if (active_ == this) active_ = nullptr;
    return false;
  }
  return true;
}

void UsbDevice::task() {
  // ponytail: SOF/ISR-driven; main-loop drain is deferred. Flags are
  // consumed by sendComplete()/takeHostLeds()/receive() polls in caller's
  // task loop. Add USBD_IRQ_Handler dispatch here when using bare metal
  // instead of RTOS interrupt trampoline.
  (void)hUsbDevice_;
  if (active_ != this) return;
}

bool UsbDevice::send(const KeyboardReport& r) {
  if (active_ != this || !configured_ || kb_tx_busy_) return false;
  kb_buf_[0] = r.modifiers;
  kb_buf_[1] = r.reserved;
  memcpy(kb_buf_ + 2, r.keys, 6);
  if (USBD_LL_Transmit(&hUsbDevice_, kKeyboardEpIn, kb_buf_, kKeyboardReportSize) != USBD_OK)
    return false;
  kb_tx_busy_ = true;
  return true;
}

bool UsbDevice::sendComplete() {
  return active_ == this && !kb_tx_busy_ && !via_tx_busy_;
}

bool UsbDevice::takeHostLeds(uint8_t& leds) {
  if (active_ != this || !led_ready_) return false;
  leds = led_buf_[0];
  led_ready_ = false;
  return true;
}

bool UsbDevice::remoteWakeup() {
  return false; // ponytail: not implemented, STM32F1 remote wakeup needs EXTI line config
}

bool UsbDevice::receive(uint8_t packet[kPacketSize]) {
  if (active_ != this || !via_rx_ready_) return false;
  memcpy(packet, via_out_buf_, kViaReportSize);
  via_rx_ready_ = false;
  return true;
}

bool UsbDevice::send(const uint8_t packet[kPacketSize]) {
  if (active_ != this || !configured_ || via_tx_busy_) return false;
  memcpy(via_in_buf_, packet, kViaReportSize);
  if (USBD_LL_Transmit(&hUsbDevice_, kViaEpIn, via_in_buf_, kViaReportSize) != USBD_OK)
    return false;
  via_tx_busy_ = true;
  return true;
}

}  // namespace stm32f1
}  // namespace via

#endif  // STM32F1XX
