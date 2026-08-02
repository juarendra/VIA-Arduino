#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

#include "VIA_Protocol.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLECharacteristic.h>
#include <NimBLEAdvertising.h>

namespace via {
namespace esp32s3 {

class BLEViaTransport : public via::Transport {
 public:
  bool begin(const char* deviceName = "AirVIA",
             uint32_t fwVersion = 0x00000001);

  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override;
  bool connected() const;

  static void onWrite(NimBLECharacteristic* c);
  static void onRead(NimBLECharacteristic* c);

 private:
  NimBLEServer* server_ = nullptr;
  NimBLEService* service_ = nullptr;
  NimBLECharacteristic* ff61_ = nullptr;
  NimBLECharacteristic* ff62_ = nullptr;

  static uint8_t rx_buffer_[kPacketSize];
  static uint8_t response_buffer_[kPacketSize];
  static volatile bool rx_ready_;
  static volatile bool tx_complete_;
  static BLEViaTransport* active_;
};

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3
