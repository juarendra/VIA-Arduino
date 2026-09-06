#pragma once
// Fake NimBLEHIDDevice for native tests. Mirrors the current NimBLE-Arduino
// (2.x) API surface used by the adapter; only behavior is simulated.
//
// The real device allocates GATT services on the server; the fake keeps a
// single input-report characteristic so the adapter's send()/notify() path can
// be asserted without polluting the server service pool.

#include <string>
#include <vector>

#include "NimBLECharacteristic.h"
#include "NimBLECore.h"
#include "NimBLEServer.h"

struct FakeNimBLE;

class NimBLEHIDDevice {
  public:
    explicit NimBLEHIDDevice(NimBLEServer* server) : server_(server) {}

    void setReportMap(uint8_t* map, uint16_t size);
    bool setManufacturer(const std::string& name);
    void setPnp(uint8_t sig, uint16_t vid, uint16_t pid, uint16_t version) {
      (void)sig;
      (void)vid;
      (void)pid;
      (void)version;
    }
    void setHidInfo(uint8_t country, uint8_t flags) {
      (void)country;
      (void)flags;
    }

    NimBLECharacteristic* getInputReport(uint8_t reportId);

  private:
    friend struct FakeNimBLE;
    NimBLEServer* server_;
    std::vector<uint8_t> reportMap_;
    NimBLECharacteristic* inputReport_ = nullptr;
    uint8_t inputReportId_ = 0;
};
