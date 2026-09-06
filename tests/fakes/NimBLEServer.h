#pragma once
#include "NimBLECore.h"
#include "NimBLEService.h"

class NimBLEAdvertising;

class NimBLEServer {
  public:
    NimBLEServer();
    ~NimBLEServer();

    void setCallbacks(NimBLEServerCallbacks* pCallbacks,
                      bool deleteCallbacks = true);
    NimBLEService* createService(const NimBLEUUID& uuid);
    bool start();
    uint8_t getConnectedCount() const { return connectedCount_; }
    NimBLEAdvertising* getAdvertising();
    NimBLEServerCallbacks* getCallbacks() const { return callbacks_; }
    int startCount() const { return startCount_; }
    std::vector<NimBLEService*>& services() { return services_; }
    const std::vector<NimBLEService*>& services() const { return services_; }

  private:
    friend struct FakeNimBLE;
    void deactivate();

    NimBLEServerCallbacks* callbacks_ = nullptr;
    uint8_t connectedCount_ = 0;
    bool started_ = false;
    int startCount_ = 0;
    std::vector<NimBLEService*> services_;
};
