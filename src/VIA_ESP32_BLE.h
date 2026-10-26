#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Keyboard.h"
#include <atomic>

#include <NimBLEAdvertising.h>
#include <NimBLECharacteristic.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEService.h>

namespace via {
namespace esp32 {

namespace {

// Standard 8-byte keyboard report (byte 0 = 8 modifier bits, byte 1 = 8
// reserved bits, bytes 2-7 = six 8-bit keycodes), matching
// via::KeyboardReport {modifiers, reserved, keys[6]}. Report ID 1 is carried
// by the GATT Report Reference descriptor, not the notification payload.
const uint8_t kReportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0xE0,  // Usage Minimum (Left Control)
    0x29, 0xE7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable) - 8 modifier bits
    0x75, 0x08,  // Report Size (8)
    0x95, 0x01,  // Report Count (1)
    0x81, 0x01,  // Input (Constant) - 8 reserved bits
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x65,  // Usage Maximum (101)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x65,  // Logical Maximum (101)
    0x75, 0x08,  // Report Size (8)
    0x95, 0x06,  // Report Count (6)
    0x81, 0x00,  // Input (Data, Array) - 6 keys
    0xC0,        // End Collection
};

const uint8_t kReportId = 1;

}  // namespace

/* NimBLE HID keyboard report adapter for generic ESP32 boards, including
 * ESP32-WROOM-32 which has no native USB.
 *
 * Hosts a standard 6-key rollover keyboard report map (report ID 1) on a
 * shared NimBLE server supplied by the sketch and publishes reports through
 * the 0x2A4D input report characteristic. The sketch owns NimBLEDevice::init,
 * createServer, server start, and advertising; begin() only attaches the HID
 * service and must run before the server is started so the GATT database is
 * complete. The NimBLEHIDDevice lives for the device lifetime (ESP32 is
 * power-reset, never torn down). */
class BleKeyboardHID : public via::KeyboardHID,
                       private NimBLECharacteristicCallbacks {
  public:
  BleKeyboardHID() = default;
  ~BleKeyboardHID() { delete hid_; }

  bool begin(NimBLEServer* server, const char* deviceName,
             const char* manufacturer) {
    if (!server || !manufacturer) return false;
    static_cast<void>(deviceName);
    server_ = server;
    hid_ = new NimBLEHIDDevice(server_);
    if (!hid_) return false;
    hid_->setManufacturer(manufacturer);
    hid_->setPnp(0x00, 0x0001, 0x0001, 0x0111);
    hid_->setHidInfo(0x00, 0x00);
    hid_->setBatteryLevel(100);
    hid_->setReportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
    inputReport_ = hid_->getInputReport(kReportId);
    for (auto& peer : subscribedPeers_) peer.store(0xFFFF, std::memory_order_relaxed);
    if (inputReport_) inputReport_->setCallbacks(this);
    return inputReport_ != nullptr;
  }

  bool configured() const override { return connected(); }

  bool send(const via::KeyboardReport& r) override {
    if (!connected() || !inputReport_) return false;
    uint8_t buf[8];
    buf[0] = r.modifiers;
    buf[1] = r.reserved;
    for (int i = 0; i < 6; ++i) buf[2 + i] = r.keys[i];
    return inputReport_->notify(buf, sizeof(buf));
  }

  bool sendComplete() override { return true; }

  bool takeHostLeds(uint8_t& leds) override {
    // BLE HID has no host LED output report.
    (void)leds;
    return false;
  }

  bool suspended() const override { return !connected(); }
  bool remoteWakeupAllowed() const override { return false; }
  bool remoteWakeup() override { return false; }

 private:
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info,
                   uint16_t value) override {
    const uint16_t handle = info.getConnHandle();
    for (auto& peer : subscribedPeers_) {
      if (peer.load(std::memory_order_relaxed) == handle) {
        if (!(value & 1U)) peer.store(0xFFFF, std::memory_order_relaxed);
        return;
      }
    }
    if (value & 1U) {
      for (auto& peer : subscribedPeers_) {
        const uint16_t current = static_cast<uint16_t>(peer.load(std::memory_order_relaxed));
        if (current == 0xFFFF || server_->getPeerMTU(current) == 0) {
          peer.store(handle, std::memory_order_relaxed);
          return;
        }
      }
    }
  }

  bool connected() const {
    if (!server_ || !inputReport_) return false;
    bool subscribed = false;
    for (auto& peer : subscribedPeers_) {
      uint32_t handle = peer.load(std::memory_order_relaxed);
      if (handle == 0xFFFF) continue;
      if (server_->getPeerMTU(static_cast<uint16_t>(handle)) == 0)
        peer.compare_exchange_strong(handle, 0xFFFF, std::memory_order_relaxed);
      else subscribed = true;
    }
    // A configuration-only central is not an HID delivery target.
    return subscribed;
  }

  NimBLEServer* server_ = nullptr;
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* inputReport_ = nullptr;
  // Native-width atomics share subscription state with NimBLE's callback task.
  mutable std::atomic<uint32_t> subscribedPeers_[NIMBLE_MAX_CONNECTIONS] = {};
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
