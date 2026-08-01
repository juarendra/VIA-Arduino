#pragma once

#ifdef STM32F1XX

#include "VIA_Keyboard.h"
#include "VIA_Protocol.h"
#include "VIA_STM32F1_USBDescriptor.h"

#include <usbd_core.h>
#include <usbd_desc.h>

namespace via {
namespace stm32f1 {

constexpr uint8_t kKeyboardEpIn = 0x81;
constexpr uint8_t kViaEpIn = 0x82;
constexpr uint8_t kViaEpOut = 0x02;
constexpr uint8_t kKeyboardReportSize = 8;
constexpr uint8_t kViaReportSize = kPacketSize;

class UsbDevice : public KeyboardHID, public Transport {
 public:
  UsbDevice();
  bool begin();
  void task();

  bool configured() const override { return configured_; }
  bool send(const KeyboardReport& r) override;
  bool sendComplete() override;
  bool takeHostLeds(uint8_t& leds) override;
  bool suspended() const override { return suspended_; }
  bool remoteWakeupAllowed() const override { return true; }
  bool remoteWakeup() override;

  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override { return !via_tx_busy_; }

 private:
  static int8_t classInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx);
  static int8_t classDeInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx);
  static int8_t classSetup(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req);
  static int8_t ep0TxSent(USBD_HandleTypeDef* pdev);
  static int8_t ep0RxReady(USBD_HandleTypeDef* pdev);
  static int8_t dataIn(USBD_HandleTypeDef* pdev, uint8_t epnum);
  static int8_t dataOut(USBD_HandleTypeDef* pdev, uint8_t epnum);
  static int8_t sof(USBD_HandleTypeDef* pdev);
  static int8_t isoInIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum);
  static int8_t isoOutIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum);

  static UsbDevice* active_;

  USBD_HandleTypeDef hUsbDevice_;
  USBD_ClassTypeDef class_;
  bool configured_;
  bool suspended_;
  uint8_t kb_buf_[kKeyboardReportSize];
  volatile bool kb_tx_busy_;
  uint8_t via_in_buf_[kViaReportSize];
  volatile bool via_tx_busy_;
  uint8_t via_out_buf_[kViaReportSize];
  volatile bool via_rx_ready_;
  uint8_t led_buf_[1];
  volatile bool led_ready_;
};

}  // namespace stm32f1
}  // namespace via

#endif  // STM32F1XX
