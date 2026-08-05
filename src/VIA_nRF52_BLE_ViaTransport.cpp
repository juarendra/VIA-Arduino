#include "VIA_nRF52_BLE_ViaTransport.h"

#if defined(ARDUINO_ARCH_NRF52) && defined(NRF52840_XXAA)

#include <cstring>

namespace via {
namespace nrf52 {

BLEViaTransport* BLEViaTransport::activeTransport_ = nullptr;

BLEViaTransport::BLEViaTransport() 
    : service_(0xFF60),
      ff61_(0xFF61),
      ff62_(0xFF62),
      pendingRequest_(false),
      droppedPackets_(0) {
    memset(requestBuffer_, 0, kPacketSize);
    memset(responseBuffer_, 0, kPacketSize);
#if defined(ARDUINO_ARCH_NRF52) && !defined(TESTING_ENVIRONMENT)
    mutex_ = xSemaphoreCreateMutexStatic(&mutexStorage_);
#else
    mutex_ = xSemaphoreCreateMutexStatic(&mutexStorage_);
#endif
}

BLEViaTransport::~BLEViaTransport() {
  if (activeTransport_ == this) activeTransport_ = nullptr;
}

bool BLEViaTransport::begin(const char* deviceName, uint32_t firmwareVersion) {
    if (!deviceName || !mutex_ || activeTransport_) return false;
    activeTransport_ = this;
    
    service_.begin();
    
    // FF61: Commands to keyboard, responses from keyboard
    ff61_.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | 
                        CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY);
    ff61_.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    ff61_.setFixedLen(kPacketSize);
    ff61_.setWriteCallback(onWrite);
    ff61_.begin();
    ff61_.write(responseBuffer_, kPacketSize); // Init read fallback buffer
    
    // FF62: Magic firmware version identifier
    uint8_t info[kPacketSize] = {0};
    info[0] = (firmwareVersion >> 24) & 0xFF;
    info[1] = (firmwareVersion >> 16) & 0xFF;
    info[2] = (firmwareVersion >> 8) & 0xFF;
    info[3] = firmwareVersion & 0xFF;
    
    size_t nameLength = strlen(deviceName);
    if (nameLength > 28) nameLength = 28;
    memcpy(info + 4, deviceName, nameLength);
    
    ff62_.setProperties(CHR_PROPS_READ);
    ff62_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS); // Read-only
    ff62_.setFixedLen(kPacketSize);
    ff62_.begin();
    ff62_.write(info, kPacketSize);
    
    return true;
}

void BLEViaTransport::onWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!activeTransport_ || len != kPacketSize) return;
    if (!xSemaphoreTake(activeTransport_->mutex_, 0)) return;
        
    if (activeTransport_->pendingRequest_) {
        activeTransport_->droppedPackets_++;
    } else {
        memcpy(activeTransport_->requestBuffer_, data, kPacketSize);
        activeTransport_->pendingRequest_ = true;
    }
    
    xSemaphoreGive(activeTransport_->mutex_);
}

bool BLEViaTransport::receive(uint8_t packet[kPacketSize]) {
    bool hasPacket = false;
    
    if (!xSemaphoreTake(mutex_, 10)) return false;
    
    if (pendingRequest_) {
        memcpy(packet, requestBuffer_, kPacketSize);
        pendingRequest_ = false;
        hasPacket = true;
    }
    xSemaphoreGive(mutex_);
    
    return hasPacket;
}

bool BLEViaTransport::send(const uint8_t packet[kPacketSize]) {
    if (!connected()) return false;
    
    // Always update the readable characteristic for polling clients
    ff61_.write(packet, kPacketSize);
    
    // If client is subscribed, push notification
    if (ff61_.notifyEnabled()) {
        return ff61_.notify(packet, kPacketSize);
    }
    
    return true; // Polling client, write was sufficient
}

bool BLEViaTransport::connected() const {
#ifdef TESTING_ENVIRONMENT
    return ::Bluefruit.connected();
#else
    return Bluefruit.connected();
#endif
}

uint32_t BLEViaTransport::droppedPackets() const {
    return droppedPackets_;
}

BLEService& BLEViaTransport::service() {
    return service_;
}

} // namespace nrf52
} // namespace via

#endif
