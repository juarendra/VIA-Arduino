#pragma once
// Fake NimBLE host primitives for native tests. Signatures and callback
// shapes mirror the current NimBLE-Arduino (2.x) headers; only the behavior
// is simulated.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// GATT characteristic property flags (real NimBLE host values).
typedef enum {
    BROADCAST = 0x01,
    READ = 0x02,
    WRITE_NR = 0x04,
    WRITE = 0x08,
    NOTIFY = 0x10,
    INDICATE = 0x20
} NIMBLE_PROPERTY;

// Opaque peer types; the transport only passes them by reference.
class NimBLEAddress {};
class NimBLEConnInfo {};

class NimBLEUUID {
  public:
    NimBLEUUID() {}
    explicit NimBLEUUID(const char* str) : str_(str ? str : "") {}
    bool operator==(const NimBLEUUID& other) const { return str_ == other.str_; }
    bool operator!=(const NimBLEUUID& other) const { return !(*this == other); }
    const std::string& str() const { return str_; }

  private:
    std::string str_;
};

class NimBLEAttValue {
  public:
    NimBLEAttValue() {}
    NimBLEAttValue(const uint8_t* data, size_t length) { setValue(data, length); }
    void setValue(const uint8_t* data, size_t length) {
      if (data == nullptr || length == 0) {
        data_.clear();
        return;
      }
      data_.assign(data, data + length);
    }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }

  private:
    std::vector<uint8_t> data_;
};

class NimBLECharacteristic;
class NimBLEServer;

class NimBLECharacteristicCallbacks {
  public:
    virtual ~NimBLECharacteristicCallbacks() {}
    virtual void onRead(NimBLECharacteristic* pCharacteristic,
                        NimBLEConnInfo& connInfo);
    virtual void onWrite(NimBLECharacteristic* pCharacteristic,
                         NimBLEConnInfo& connInfo);
    virtual void onStatus(NimBLECharacteristic* pCharacteristic,
                          NimBLEConnInfo& connInfo, int code);
    virtual void onSubscribe(NimBLECharacteristic* pCharacteristic,
                             NimBLEConnInfo& connInfo, uint16_t subValue);
};

class NimBLEServerCallbacks {
  public:
    virtual ~NimBLEServerCallbacks() {}
    virtual void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo);
    virtual void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                              int reason);
};
