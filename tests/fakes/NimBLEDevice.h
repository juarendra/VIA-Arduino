#pragma once
#include <string>
#include "NimBLECore.h"
#include "NimBLEServer.h"
#include "NimBLEService.h"
#include "NimBLECharacteristic.h"
#include "NimBLEAdvertising.h"

class NimBLEDevice {
  public:
    static bool init(const std::string& deviceName);
    static NimBLEServer* createServer();
    static NimBLEServer* getServer();
};
