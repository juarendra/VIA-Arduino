/*
 * ESP32-S3 Wireless VIA Keyboard reference for VIA_Arduino.
 *
 * SoC: ESP32-S3-WROOM-1 (custom PCB)
 * Core: espressif/esp32 v3.0.x, USB Mode: TinyUSB
 *
 * 5 rows x 15 columns x 4 layers, COL2ROW, active-low
 * Dual-mode: USB for VIA config + wired typing, BLE for wireless typing
 * This example compiles but is NOT hardware-verified.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <BleKeyboard.h>
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
#include <VIA_TinyUSB_RawHID.h>
#include <VIA_TinyUSB_Keyboard.h>

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

via::Config protocolConfig = {
    5, 15, 4, keymap, defaultKeymap,
    nullptr, 0, 0, 1, 750
};

// --- Persistence ---
via::esp32s3::NVSStorage nvs;

// --- USB ---
via::tinyusb::RawHID viaRawHid;
via::tinyusb::Keyboard usbKeyboard;

// --- Protocol ---
via::Protocol protocol(protocolConfig, viaRawHid, &nvs);

// --- BLE ---
BleKeyboard bleKeyboard("VIA Keyboard", "VIA-Arduino", 100);
via::esp32s3::BleKeyboardHID bleHid(bleKeyboard);
via::esp32s3::BLEViaTransport bleVia;

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

// --- State ---
static bool usbActive = false;

// --- Setup ---
void setup() {
  nvs.begin();
  viaRawHid.begin("VIA Raw HID");
  usbKeyboard.begin("VIA Keyboard");
  bleVia.begin("AirVIA KB", 0x00000001);
  protocol.begin(millis());
  bleKeyboard.begin();
  keyboard.begin();
  battery.setCalibration(3200, 4200);
  sleepMgr.configure(300000);
  sleepMgr.update(true, millis());

  usbActive = TinyUSBDevice.mounted();
}

// --- Loop ---
void loop() {
  uint32_t now = millis();
  bool mounted = TinyUSBDevice.mounted();

  if (mounted != usbActive) {
    usbActive = mounted;
    // ponytail: swap HID transport at mode change
    keyboard = via::Keyboard({5, 15}, matrix, protocol,
                              usbActive ? static_cast<via::KeyboardHID&>(usbKeyboard)
                                        : static_cast<via::KeyboardHID&>(bleHid),
                              activeCodes, &keyboardCallbacks);
    keyboard.begin();
  }

  // VIA over BLE (AirVIA transport)
  uint8_t blePacket[via::kPacketSize];
  if (bleVia.receive(blePacket)) {
    protocol.process(blePacket, now);
    bleVia.send(blePacket);
  }

  protocol.task(now);
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
