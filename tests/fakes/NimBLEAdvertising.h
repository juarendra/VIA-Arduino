#pragma once
#include "NimBLECore.h"

class NimBLEAdvertising {
  public:
    NimBLEAdvertising() {}
    ~NimBLEAdvertising();

    bool start(uint32_t duration = 0, const NimBLEAddress* dirAddr = nullptr);
    bool addServiceUUID(const NimBLEUUID& serviceUUID);
    const std::vector<std::string>& serviceUuids() const {
      return serviceUuids_;
    }

  private:
    friend struct FakeNimBLE;
    std::vector<std::string> serviceUuids_;
};
