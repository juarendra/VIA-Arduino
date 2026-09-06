#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Keyboard.h"

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
// via::KeyboardReport {modifiers, reserved, keys[6]}. Report ID 1, so the
// notify payload is [id, modifiers, reserved, keys[0..5]] (9 bytes).
const uint8_t kReportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0xE2,  // Usage Minimum (0xE2)
    0x29, 0xFD,  // Usage Maximum (0xFD)
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
 * Hosts a standard 6-key rollover keyboard report map (report ID 1) on the
 * shared NimBLE server and publishes reports through the 0x2A4D input report
 * characteristic. begin() must run after NimBLEDevice::init() and before the
 * server is started (BLEViaTransport::begin() calls server start), so the HID
 * service is present when the GATT database is built. The NimBLEHIDDevice
 * lives for the device lifetime (ESP32 is power-reset, never torn down). */
class BleKeyboardHID : public via::KeyboardHID {
 public:
  BleKeyboardHID() = default;

  bool begin(const char* deviceName, const char* manufacturer) {
    server_ = NimBLEDevice::createServer();
    if (!server_) return false;
    hid_ = new NimBLEHIDDevice(server_);
    hid_->setManufacturer(manufacturer);
    hid_->setReportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
    inputReport_ = hid_->getInputReport(kReportId);
    if (NimBLEAdvertising* adv = server_->getAdvertising()) {
      adv->setName(deviceName);
      adv->addServiceUUID(NimBLEUUID("0x1812"));
    }
    return inputReport_ != nullptr;
  }

  bool configured() const override { return connected(); }

  bool send(const via::KeyboardReport& r) override {
    if (!connected() || !inputReport_) return false;
    uint8_t buf[9];
    buf[0] = kReportId;
    buf[1] = r.modifiers;
    buf[2] = r.reserved;
    for (int i = 0; i < 6; ++i) buf[3 + i] = r.keys[i];
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
  bool connected() const { return server_ && server_->getConnectedCount() > 0; }

  NimBLEServer* server_ = nullptr;
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* inputReport_ = nullptr;
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
