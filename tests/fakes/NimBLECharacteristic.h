#pragma once
#include "NimBLECore.h"

class NimBLECharacteristic : public NimBLEAttValue {
  public:
    NimBLECharacteristic();
    NimBLECharacteristic(const NimBLEUUID& uuid,
                         uint32_t properties =
                             NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE,
                         uint16_t maxLen = 512);
    ~NimBLECharacteristic();

    NimBLEAttValue getValue() const { return *this; }
    bool notify(const uint8_t* data, size_t length,
                uint16_t connHandle = 0xFFFF) const;
    void setCallbacks(NimBLECharacteristicCallbacks* pCallbacks);
    NimBLECharacteristicCallbacks* getCallbacks() const { return callbacks_; }
    uint32_t getProperties() const { return properties_; }
    const NimBLEUUID& uuid() const { return uuid_; }
    bool active() const { return active_; }

  private:
    friend class NimBLEService;
    friend struct FakeNimBLE;
    void activate(const NimBLEUUID& uuid, uint32_t properties, uint16_t maxLen);
    void deactivate();

    NimBLEUUID uuid_;
    uint32_t properties_ = 0;
    uint16_t maxLen_ = 0;
    NimBLECharacteristicCallbacks* callbacks_ = nullptr;
    bool active_ = false;
};
