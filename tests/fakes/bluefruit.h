#pragma once
#include <stdint.h>
#include <functional>
#include <cstring>

#ifndef CHR_PROPS_READ
#define CHR_PROPS_READ 1
#define CHR_PROPS_WRITE 2
#define CHR_PROPS_WRITE_WO_RESP 4
#define CHR_PROPS_NOTIFY 8
#define SECMODE_OPEN 1
#define SECMODE_NO_ACCESS 2
#endif

typedef void (*WriteCallback_t)(uint16_t, uint8_t*, uint16_t);

class BLEService {
public:
    BLEService(uint16_t uuid = 0) : uuid(uuid) {}
    void begin();
    uint16_t uuid;
};

class BLECharacteristic {
public:
    BLECharacteristic(uint16_t uuid = 0) : uuid(uuid) {}
    void setProperties(uint8_t p) { props = p; }
    void setPermission(uint8_t read, uint8_t write) { readPerm = read; writePerm = write; }
    void setFixedLen(uint16_t len) { fixedLen = len; }
    void setWriteCallback(WriteCallback_t cb) { writeCb = cb; }
    void begin();
    uint16_t write(const void* data, uint16_t len);
    bool notify(const void* data, uint16_t len);
    bool notifyEnabled();

    uint16_t uuid;
    uint8_t props;
    uint8_t readPerm;
    uint8_t writePerm;
    uint16_t fixedLen;
    WriteCallback_t writeCb = nullptr;
};


struct BLEHidAdafruit {
  uint8_t lastModifier = 0;
  uint8_t lastKeys[6] = {0};
  std::function<void(uint16_t, uint8_t)> ledCallback;

  bool keyboardReport(uint8_t modifier, const uint8_t keycode[6]) {
    lastModifier = modifier;
    for(int i=0; i<6; i++) lastKeys[i] = keycode[i];
    return true;
  }
  
  void setKeyboardLedCallback(void (*cb)(uint16_t, uint8_t)) {
      ledCallback = cb;
  }

  void dispatchKeyboardLeds(uint8_t leds) {
    if (ledCallback) ledCallback(0, leds);
  }
};

struct FakeBluefruit {
  bool connectedResult = false;
  bool connected() { return connectedResult; }
  
  static uint16_t serviceUuid;
  static uint16_t ff61Uuid;
  static uint16_t ff61FixedLength;
  static uint16_t ff62Uuid;
  static uint8_t ff62Value[32];
  
  static WriteCallback_t ff61Cb;
  static uint8_t ff61Written[32];
  static bool ff61Notifying;
  static bool ff61NotifySuccess;
  static int ff61NotifyCount;

  static void reset() {
      serviceUuid = 0;
      ff61Uuid = 0;
      ff61FixedLength = 0;
      ff62Uuid = 0;
      memset(ff62Value, 0, sizeof(ff62Value));
      ff61Cb = nullptr;
      memset(ff61Written, 0, sizeof(ff61Written));
      ff61Notifying = false;
      ff61NotifySuccess = false;
      ff61NotifyCount = 0;
  }
  
  static bool dispatchWrite(const uint8_t* data, uint16_t len) {
      if (len != 32) return false;
      if (ff61Cb) {
          // Const cast for test mock only
          ff61Cb(0, (uint8_t*)data, len);
          return true;
      }
      return false;
  }
};
extern FakeBluefruit Bluefruit;
