#include "VIA_ESP32S3_BLE_ViaTransport.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

#include <string.h>

namespace via {
namespace esp32s3 {

static constexpr NimBLEUUID kServiceFF60("0000FF60-0000-1000-8000-00805F9B34FB");
static constexpr NimBLEUUID kCharFF61(  "0000FF61-0000-1000-8000-00805F9B34FB");
static constexpr NimBLEUUID kCharFF62(  "0000FF62-0000-1000-8000-00805F9B34FB");

uint8_t BLEViaTransport::rx_buffer_[kPacketSize] = {};
uint8_t BLEViaTransport::response_buffer_[kPacketSize] = {};
volatile bool BLEViaTransport::rx_ready_ = false;
volatile bool BLEViaTransport::tx_complete_ = true;
BLEViaTransport* BLEViaTransport::active_ = nullptr;

// ponytail: NimBLE characteristic callback adapter
class CharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    BLEViaTransport::onWrite(c);
  }
  void onRead(NimBLECharacteristic* c) override {
    BLEViaTransport::onRead(c);
  }
};

static CharCallbacks char_cb_;

bool BLEViaTransport::begin(const char* deviceName, uint32_t fwVersion) {
  if (active_) return false;
  active_ = this;

  NimBLEDevice::init(deviceName);

  server_ = NimBLEDevice::createServer();
  service_ = server_->createService(kServiceFF60);

  ff61_ = service_->createCharacteristic(kCharFF61,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  ff61_->setCallbacks(&char_cb_);

  uint8_t info[32] = {};
  info[0] = static_cast<uint8_t>(fwVersion >> 24);
  info[1] = static_cast<uint8_t>(fwVersion >> 16);
  info[2] = static_cast<uint8_t>(fwVersion >> 8);
  info[3] = static_cast<uint8_t>(fwVersion);
  size_t nameLen = strlen(deviceName);
  if (nameLen > 28) nameLen = 28;
  memcpy(&info[4], deviceName, nameLen);

  ff62_ = service_->createCharacteristic(kCharFF62, NIMBLE_PROPERTY::READ);
  ff62_->setValue(info, 32);

  service_->start();

  NimBLEAdvertising* adv = server_->getAdvertising();
  adv->addServiceUUID(kServiceFF60);
  adv->start();

  return true;
}

bool BLEViaTransport::receive(uint8_t packet[kPacketSize]) {
  if (!active_ || active_ != this) return false;
  if (!rx_ready_) return false;
  memcpy(packet, rx_buffer_, kPacketSize);
  rx_ready_ = false;
  return true;
}

bool BLEViaTransport::send(const uint8_t packet[kPacketSize]) {
  if (!active_ || active_ != this) return false;
  if (!ff61_ || !ff61_->getSubscribedCount()) return false;
  memcpy(response_buffer_, packet, kPacketSize);
  ff61_->notify(response_buffer_, kPacketSize);
  return true;
}

bool BLEViaTransport::sendComplete() {
  // ponytail: NimBLE notify is fire-and-forget; no per-transfer completion
  return true;
}

bool BLEViaTransport::connected() const {
  return server_ && server_->getConnectedCount() > 0;
}

void BLEViaTransport::onWrite(NimBLECharacteristic*) {
  if (!active_ || !active_->ff61_) return;
  NimBLEAttValue val = active_->ff61_->getValue();
  if (val.size() >= kPacketSize) {
    memcpy(rx_buffer_, val.data(), kPacketSize);
    rx_ready_ = true;
  }
}

void BLEViaTransport::onRead(NimBLECharacteristic*) {
  // FF62 value is set once at begin(); FF61 read returns last response
  if (active_ && active_->ff61_) {
    active_->ff61_->setValue(response_buffer_, kPacketSize);
  }
}

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3
