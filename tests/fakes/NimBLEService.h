#pragma once
#include "NimBLECore.h"
#include "NimBLECharacteristic.h"

class NimBLEService {
  public:
    NimBLEService() {}
    NimBLEService(const NimBLEUUID& uuid) : uuid_(uuid) {}
    ~NimBLEService();

    NimBLECharacteristic* createCharacteristic(
        const NimBLEUUID& uuid,
        uint32_t properties =
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE,
        uint16_t maxLen = 512);
    // No-op in current NimBLE (services start with the server); kept for
    // source compatibility with older call patterns.
    bool start() { return true; }
    const NimBLEUUID& uuid() const { return uuid_; }

  private:
    friend class NimBLEServer;
    friend struct FakeNimBLE;
    void activate(const NimBLEUUID& uuid);
    void deactivate();

    NimBLEUUID uuid_;
};
