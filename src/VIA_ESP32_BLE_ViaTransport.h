#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "VIA_Protocol.h"

#if !defined(TESTING_ENVIRONMENT)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
// Fake FreeRTOS for tests
struct StaticSemaphore_t {};
typedef void* SemaphoreHandle_t;
extern bool g_fake_mutex_take;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
  return storage;
}
inline bool xSemaphoreTake(SemaphoreHandle_t, int) { return g_fake_mutex_take; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
#endif

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEService.h>
#include <NimBLECharacteristic.h>
#include <NimBLEAdvertising.h>

namespace via {
namespace esp32 {

/* VIA transport over BLE GATT for generic ESP32 boards (WROOM-32, S3).
 *
 * Built against the current NimBLE-Arduino (2.x) API: characteristic
 * callbacks receive a NimBLEConnInfo, subscriptions are tracked through
 * onSubscribe(), and services start together with the server.
 *
 * GATT layout (128-bit AirVIA UUIDs):
 *   Service  FF60  0000FF60-0000-1000-8000-00805F9B34FB
 *   FF61     READ | WRITE | WRITE_NR | NOTIFY
 *            Client writes 32-byte VIA commands; the keyboard answers with
 *            a 32-byte response via notify (or readable value for polling
 *            clients).
 *   FF62     READ
 *            Firmware identity: big-endian uint32 version, then zero-padded
 *            device name.
 *
 * Characteristic callbacks run in the NimBLE host task; packet handling is
 * deferred to receive(), which the sketch calls from the main loop, so the
 * VIA protocol never runs inside a BLE callback. A zero-timeout mutex guards
 * the pending request slot; commands that arrive while a previous command is
 * still pending are counted as dropped instead of overwriting it. */
class BLEViaTransport : public via::Transport {
 public:
  BLEViaTransport();
  ~BLEViaTransport() override;

  bool begin(NimBLEServer* server, const char* deviceName = "AirVIA",
             uint32_t fwVersion = 0x00000001);

  bool receive(uint8_t packet[kPacketSize]) override;
  bool send(const uint8_t packet[kPacketSize]) override;
  bool sendComplete() override { return true; }

  bool connected() const;
  uint32_t droppedPackets() const;
  // Number of peers currently subscribed to FF61 notifications.
  uint8_t subscribers() const { return notifySubscribers_; }

  // Invoked by the NimBLE adapter classes; safe to call from any task.
  static void onWrite(NimBLECharacteristic* c);
  static void onSubscribe(NimBLECharacteristic* c, uint16_t subValue);
  static void onDisconnect();

 private:
  NimBLEServer* server_ = nullptr;
  NimBLEService* service_ = nullptr;
  NimBLECharacteristic* ff61_ = nullptr;
  NimBLECharacteristic* ff62_ = nullptr;

  StaticSemaphore_t mutexStorage_;
  SemaphoreHandle_t mutex_ = nullptr;

  uint8_t requestBuffer_[kPacketSize];
  volatile bool pendingRequest_ = false;
  volatile uint32_t droppedPackets_ = 0;
  volatile uint8_t notifySubscribers_ = 0;

  static BLEViaTransport* activeTransport_;
};

}  // namespace esp32
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32
