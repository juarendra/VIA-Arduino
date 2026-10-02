#pragma once
#include "NimBLECore.h"

class NimBLEAdvertising {
  public:
    NimBLEAdvertising() {}
    ~NimBLEAdvertising();

    bool start(uint32_t duration = 0, const NimBLEAddress* dirAddr = nullptr);
    bool addServiceUUID(const NimBLEUUID& serviceUUID);
    bool setName(const std::string& name);
    const std::vector<std::string>& serviceUuids() const {
      return serviceUuids_;
    }
    const std::string& name() const { return name_; }

  private:
    friend struct FakeNimBLE;
    std::vector<std::string> serviceUuids_;
    std::string name_;
};
