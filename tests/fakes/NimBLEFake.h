#pragma once
// Test control surface for the fake NimBLE host. Mirrors the FakeBluefruit
// pattern used by the nRF52 tests: static state + static helpers.

#include "NimBLECharacteristic.h"
#include "NimBLECore.h"
#include "NimBLEDevice.h"
#include "NimBLEService.h"

struct FakeNimBLE {
  // Observed state.
  static std::string deviceName;
  static int initCount;
  static int createServerCount;
  static int serverStartCount;
  static int advertisingStartCount;
  static std::vector<std::string> advertisedServiceUuids;
  static bool notifySuccess;
  static int notifyCount;
  static std::vector<uint8_t> lastNotified;

  // Pool objects (fixed storage; no dynamic allocation, ASan-clean).
  static NimBLEServer serverObj;
  static NimBLEAdvertising advertisingObj;
  static NimBLEService servicePool[4];
  static NimBLECharacteristic charPool[8];
  static NimBLEConnInfo connInfoObj;
  static int serviceIndex;
  static int charIndex;

  static void reset();
  static NimBLECharacteristic* findChar(const char* uuidSuffix);
  static NimBLEService* findService(const char* uuidSuffix);
  static NimBLEService* serviceForChar(const NimBLECharacteristic* chr);
  static bool dispatchWrite(const uint8_t* data, size_t len);
  static void dispatchSubscribe(uint16_t subValue);
  static void connect();
  static void disconnect();
};

extern FakeNimBLE Nimble;
