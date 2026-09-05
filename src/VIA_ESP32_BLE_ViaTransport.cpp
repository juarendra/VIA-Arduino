#include "VIA_ESP32_BLE_ViaTransport.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <string.h>

namespace via {
namespace esp32 {

BLEViaTransport* BLEViaTransport::activeTransport_ = nullptr;

// 128-bit AirVIA GATT identifiers.
static const NimBLEUUID kServiceFF60("0000FF60-0000-1000-8000-00805F9B34FB");
static const NimBLEUUID kCharFF61("0000FF61-0000-1000-8000-00805F9B34FB");
static const NimBLEUUID kCharFF62("0000FF62-0000-1000-8000-00805F9B34FB");

// NimBLE characteristic callback adapter (NimBLE 2.x passes the peer
// connection info alongside every event). The handler work is deferred to
// BLEViaTransport::onWrite/onSubscribe, which are safe from the host task.
class CharCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    BLEViaTransport::onWrite(c);
  }
  void onSubscribe(NimBLECharacteristic* c, NimBLEConnInfo& connInfo,
                   uint16_t subValue) override {
    (void)connInfo;
    BLEViaTransport::onSubscribe(c, subValue);
  }
};

// Disconnect cleanup: clear the pending request so a command captured from a
// dead connection is never replayed to a new one.
class ServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                    int reason) override {
    (void)pServer;
    (void)connInfo;
    (void)reason;
    BLEViaTransport::onDisconnect();
  }
};

static CharCallbacks char_cb_;
static ServerCallbacks server_cb_;

BLEViaTransport::BLEViaTransport() {
  mutex_ = xSemaphoreCreateMutexStatic(&mutexStorage_);
  memset(requestBuffer_, 0, sizeof(requestBuffer_));
}

BLEViaTransport::~BLEViaTransport() {
  if (activeTransport_ == this) activeTransport_ = nullptr;
}

bool BLEViaTransport::begin(const char* deviceName, uint32_t fwVersion) {
  if (!deviceName || !mutex_ || activeTransport_) return false;
  activeTransport_ = this;

  NimBLEDevice::init(deviceName);

  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(&server_cb_, false);

  service_ = server_->createService(kServiceFF60);

  // FF61: commands to the keyboard, responses from the keyboard.
  ff61_ = service_->createCharacteristic(
      kCharFF61,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  ff61_->setCallbacks(&char_cb_);

  uint8_t zeros[kPacketSize] = {};
  ff61_->setValue(zeros, kPacketSize);  // initial read fallback buffer

  // FF62: big-endian uint32 firmware version, then zero-padded device name.
  uint8_t info[kPacketSize] = {};
  info[0] = static_cast<uint8_t>(fwVersion >> 24);
  info[1] = static_cast<uint8_t>(fwVersion >> 16);
  info[2] = static_cast<uint8_t>(fwVersion >> 8);
  info[3] = static_cast<uint8_t>(fwVersion);
  size_t nameLen = strlen(deviceName);
  if (nameLen > 28) nameLen = 28;
  memcpy(&info[4], deviceName, nameLen);
  ff62_ = service_->createCharacteristic(kCharFF62, NIMBLE_PROPERTY::READ);
  ff62_->setValue(info, kPacketSize);

  // NimBLE 2.x starts all services together with the server.
  server_->start();

  NimBLEAdvertising* adv = server_->getAdvertising();
  adv->addServiceUUID(kServiceFF60);
  adv->start();

  return true;
}

void BLEViaTransport::onWrite(NimBLECharacteristic* c) {
  BLEViaTransport* t = activeTransport_;
  if (!t || c != t->ff61_) return;
  NimBLEAttValue value = c->getValue();
  if (value.size() != kPacketSize) return;  // fixed 32-byte packets only

  if (!xSemaphoreTake(t->mutex_, 0)) return;
  if (t->pendingRequest_) {
    t->droppedPackets_++;
  } else {
    memcpy(t->requestBuffer_, value.data(), kPacketSize);
    t->pendingRequest_ = true;
  }
  xSemaphoreGive(t->mutex_);
}

void BLEViaTransport::onSubscribe(NimBLECharacteristic* c, uint16_t subValue) {
  BLEViaTransport* t = activeTransport_;
  if (!t || c != t->ff61_) return;
  // subValue bit 0 is the CCC notify subscription (BLE spec).
  if (subValue & 0x01) {
    if (t->notifySubscribers_ < 255) t->notifySubscribers_++;
  } else if (t->notifySubscribers_ > 0) {
    t->notifySubscribers_--;
  }
}

void BLEViaTransport::onDisconnect() {
  BLEViaTransport* t = activeTransport_;
  if (!t) return;
  if (!xSemaphoreTake(t->mutex_, 0)) return;
  t->pendingRequest_ = false;
  xSemaphoreGive(t->mutex_);
}

bool BLEViaTransport::receive(uint8_t packet[kPacketSize]) {
  if (!xSemaphoreTake(mutex_, 10)) return false;
  bool hasPacket = false;
  if (pendingRequest_) {
    memcpy(packet, requestBuffer_, kPacketSize);
    pendingRequest_ = false;
    hasPacket = true;
  }
  xSemaphoreGive(mutex_);
  return hasPacket;
}

bool BLEViaTransport::send(const uint8_t packet[kPacketSize]) {
  if (!connected() || !ff61_) return false;

  // Always update the readable value for polling clients.
  ff61_->setValue(packet, kPacketSize);

  if (notifySubscribers_ > 0) {
    return ff61_->notify(packet, kPacketSize);
  }
  return true;  // polling client; the readable value is sufficient
}

bool BLEViaTransport::connected() const {
  return server_ && server_->getConnectedCount() > 0;
}

uint32_t BLEViaTransport::droppedPackets() const {
  return droppedPackets_;
}

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
