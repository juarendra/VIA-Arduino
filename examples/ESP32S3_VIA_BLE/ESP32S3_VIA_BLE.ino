/*
 * ESP32-S3 Wireless VIA Keyboard reference for VIA_Arduino.
 *
 * SoC: ESP32-S3-WROOM-1 (custom PCB)
 * Core: espressif/esp32 v3.0.x or newer, NimBLE-Arduino 2.x (2.5.x)
 *
 * 5 rows x 15 columns x 4 layers, COL2ROW, active-low
 * VIA configuration and typing both go over BLE (AirVIA FF60 GATT).
 * This example compiles but is NOT hardware-verified.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_Encoder.h>
#include <VIA_Battery.h>
#include <VIA_SleepMgr.h>
#include <VIA_ESP32S3_GPIO.h>
#include <VIA_ESP32S3_NVS.h>
#include <VIA_ESP32S3_BLE.h>
#include <VIA_ESP32S3_BLE_ViaTransport.h>

// --- Matrix: 5 rows x 15 cols ---
static const via::Pin rowPins[5]  = {5, 6, 7, 15, 16};
static const via::Pin colPins[15] = {
    8, 9, 10, 11, 12, 13, 14, 17,
    18, 19, 20, 21, 39, 40, 41
};

static uint32_t rawRows[5]       = {};
static uint32_t candidateRows[5] = {};
static uint32_t stableRows[5]    = {};
static uint32_t changedRows[5]   = {};

via::esp32s3::MatrixIOArduino matrixIO;

via::MatrixConfig matrixConfig = {
    5, 15, rowPins, colPins, via::kColToRow, 30, 5,
    rawRows, candidateRows, stableRows, changedRows
};

via::Matrix matrix(matrixConfig, matrixIO);

// --- Keymap ---
static uint16_t keymap[5 * 15 * 4]        = {};
static const uint16_t defaultKeymap[5 * 15 * 4] = {};

// --- Persistence ---
via::esp32s3::NVSStorage nvs;

// --- BLE ---
via::esp32s3::BleKeyboardHID bleHid;
via::esp32s3::BLEViaTransport bleVia;

// --- Protocol ---
// Required staging space = keymap + layoutOptions (4 bytes),
// per Protocol::requiredLoadBufferSize().
static uint8_t loadBuffer[sizeof(keymap) + sizeof(uint32_t)] = {};

via::Config protocolConfig = {
    5, 15, 4, keymap, defaultKeymap,
    nullptr, 0, 0, 0x00000001, 750, 0, 0, nullptr, nullptr,
    loadBuffer, sizeof(loadBuffer)
};

via::Protocol protocol(protocolConfig, bleVia, &nvs);

// --- Active Codes ---
static uint16_t activeCodes[5 * 15] = {};

// --- Keyboard ---
static via::KeyboardCallbacks keyboardCallbacks;

via::Keyboard keyboard({5, 15}, matrix, protocol, bleHid,
                       activeCodes, &keyboardCallbacks);

// --- Encoder ---
via::Encoder encoder;

// --- Battery ---
via::BatteryMgr battery;

// --- Sleep ---
via::SleepMgr sleepMgr;

// --- Setup ---
void setup() {
  if (!nvs.begin()) return;
  NimBLEDevice::init("AirVIA S3");
  NimBLEServer* server = NimBLEDevice::createServer();
  if (!server) return;
  if (!bleHid.begin(server, "AirVIA S3", "VIA-Arduino")) return;
  if (!bleVia.begin(server, "AirVIA S3", 0x00000001)) return;
  server->start();
  NimBLEAdvertising* adv = server->getAdvertising();
  if (!adv) return;
  adv->addServiceUUID(NimBLEUUID("0x1812"));
  adv->addServiceUUID(NimBLEUUID(
      "0000FF60-0000-1000-8000-00805F9B34FB"));
  adv->setName("AirVIA S3");
  adv->start();
  if (!protocol.begin(millis())) return;
  if (!keyboard.begin()) return;
  battery.setCalibration(3200, 4200);
  sleepMgr.configure(300000);
  sleepMgr.update(true, millis());
}

// --- Loop ---
void loop() {
  uint32_t now = millis();
  protocol.task(now);
  matrix.task(now);
  keyboard.task(now);

  encoder.update(digitalRead(4), digitalRead(3), now);
  int32_t encDelta = encoder.consume();
  (void)encDelta; // ponytail: encoder injection deferred to event pipeline

  battery.update(analogRead(47), now);
  sleepMgr.update(!matrix.hasChanged(), now);

  if (sleepMgr.sleepRequested()) {
    protocol.save();
    esp_deep_sleep_start();
  }
}