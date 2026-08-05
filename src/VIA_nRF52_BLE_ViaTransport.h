#pragma once

#include "VIA_Protocol.h"
#include <bluefruit.h>

#if defined(ARDUINO_ARCH_NRF52) && !defined(TESTING_ENVIRONMENT)
#include "FreeRTOS.h"
#include "semphr.h"
#else
// Fake FreeRTOS for tests
typedef void* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline bool xSemaphoreTake(SemaphoreHandle_t, int) { return true; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
#endif

namespace via {
namespace nrf52 {

class BLEViaTransport : public via::Transport {
 public:
  BLEViaTransport();
  
  bool begin(const char* deviceName = "AirVIA", uint32_t firmwareVersion = 1);
  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override { return true; }
  
  bool connected() const;
  uint32_t droppedPackets() const;
  BLEService& service();

 private:
  static void onWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);

  BLEService service_;
  BLECharacteristic ff61_;
  BLECharacteristic ff62_;
  
  uint8_t requestBuffer_[kPacketSize];
  uint8_t responseBuffer_[kPacketSize];
  
  SemaphoreHandle_t mutex_;
  volatile bool pendingRequest_;
  volatile uint32_t droppedPackets_;
  
  static BLEViaTransport* activeTransport_;
};

} // namespace nrf52
} // namespace via
