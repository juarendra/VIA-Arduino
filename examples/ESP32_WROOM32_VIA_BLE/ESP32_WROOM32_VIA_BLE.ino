/*
 * ESP32-WROOM-32 Wireless VIA Keyboard reference for VIA_Arduino.
 *
 * SoC: ESP32-WROOM-32 (no native USB - BLE only)
 * Core: espressif/esp32 v3.0.x or newer, NimBLE-Arduino 2.x (2.5.x)
 *
 * 5 rows x 6 columns x 3 layers, COL2ROW, active-low
 * 1 rotary encoder (GPIO 33 clock, GPIO 32 detent)
 * VIA configuration and typing both go over BLE (AirVIA FF60 GATT).
 * This example compiles but is NOT hardware-verified.
 *
 * Defaults avoid classic ESP32 strapping GPIO0/2/5/12/15, flash GPIO6-11,
 * and input-only GPIO34-39. GPIO16/17 require WROOM without PSRAM.
 * Re-wire rowPins/colPins/encoder pins to match your PCB.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_Encoder.h>
#include <VIA_ESP32_GPIO.h>
#include <VIA_ESP32_NVS.h>
#include <VIA_ESP32_BLE.h>
#include <VIA_ESP32_BLE_ViaTransport.h>
#include "WroomOTA.h"


// --- Matrix: 5 rows x 6 cols ---
static const via::Pin rowPins[5] = {4, 19, 23, 13, 14};
static const via::Pin colPins[6] = {27, 16, 17, 18, 21, 22};

static constexpr uint8_t ROWS = 5;
static constexpr uint8_t COLS = 6;
static constexpr uint8_t LAYERS = 3;

static uint32_t rawRows[ROWS]       = {};
static uint32_t candidateRows[ROWS] = {};
static uint32_t stableRows[ROWS]    = {};
static uint32_t changedRows[ROWS]   = {};

via::esp32::MatrixIOArduino matrixIO;

via::MatrixConfig matrixConfig = {
    ROWS, COLS, rowPins, colPins, via::kColToRow, 30, 5,
    rawRows, candidateRows, stableRows, changedRows
};

via::Matrix matrix(matrixConfig, matrixIO);

// --- Keycodes (from QMK naming) ---
static constexpr uint16_t KC_TRNS = 0x0001;
static constexpr uint16_t KC_A = 0x0004;
static constexpr uint16_t KC_S = 0x0016;
static constexpr uint16_t KC_D = 0x0007;
static constexpr uint16_t KC_F = 0x0009;
static constexpr uint16_t KC_G = 0x000A;
static constexpr uint16_t KC_H = 0x000B;
static constexpr uint16_t KC_Z = 0x001D;
static constexpr uint16_t KC_X = 0x001B;
static constexpr uint16_t KC_C = 0x0006;
static constexpr uint16_t KC_V = 0x0019;
static constexpr uint16_t KC_B = 0x0005;
static constexpr uint16_t KC_N = 0x0011;
static constexpr uint16_t KC_Q = 0x0014;
static constexpr uint16_t KC_W = 0x001A;
static constexpr uint16_t KC_E = 0x0008;
static constexpr uint16_t KC_R = 0x0015;
static constexpr uint16_t KC_T = 0x0017;
static constexpr uint16_t KC_Y = 0x001C;
static constexpr uint16_t KC_1 = 0x001E;
static constexpr uint16_t KC_2 = 0x001F;
static constexpr uint16_t KC_3 = 0x0020;
static constexpr uint16_t KC_4 = 0x0021;
static constexpr uint16_t KC_5 = 0x0022;
static constexpr uint16_t KC_6 = 0x0023;
static constexpr uint16_t KC_F1 = 0x003A;
static constexpr uint16_t KC_F2 = 0x003B;
static constexpr uint16_t KC_F3 = 0x003C;
static constexpr uint16_t KC_F4 = 0x003D;
static constexpr uint16_t KC_F5 = 0x003E;
static constexpr uint16_t KC_F6 = 0x003F;
static constexpr uint16_t KC_VOLU = 0x00E2;
static constexpr uint16_t KC_VOLD = 0x00E4;
static constexpr uint16_t KC_MUTE = 0x00E7;
static constexpr uint16_t MO_1 = 0x5221;
static constexpr uint16_t DF_0 = 0x5260;

// --- Keymap ---
static uint16_t keymap[LAYERS * ROWS * COLS] = {};
static const uint16_t defaultKeymap[LAYERS * ROWS * COLS] = {
    // Layer 0
    KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y,
    KC_A, KC_S, KC_D, KC_F, KC_G, KC_H,
    KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N,
    KC_1, KC_2, KC_3, KC_4, KC_5, KC_6,
    MO_1, DF_0, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    // Layer 1
    KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    DF_0, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    // Layer 2
    KC_VOLU, KC_VOLD, KC_MUTE, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    DF_0, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
};

// --- Encoder ---
// encoderMap is sized by the protocol: layers * encoderCount * 2 (CW/CCW per
// layer). Only layer 0 gets keys; other layers stay 0 (no binding).
static const via::Pin kEncoderClockPin = 33;
static const via::Pin kEncoderDetentPin = 32;
via::Encoder encoder;
static uint16_t encoderMap[LAYERS * 2] = {};
static const uint16_t defaultEncoderMap[LAYERS * 2] = {KC_VOLU, KC_VOLD};

// --- Protocol ---
// Required staging space = keymap + encoderMap + layoutOptions (4 bytes),
// per Protocol::requiredLoadBufferSize().
static uint8_t loadBuffer[sizeof(keymap) + sizeof(encoderMap) + sizeof(uint32_t)] = {};

via::Config protocolConfig = {
    ROWS, COLS, LAYERS, keymap, defaultKeymap,
    nullptr, 0, 0, 0x00000001, 750, 0, 1, encoderMap, defaultEncoderMap,
    loadBuffer, sizeof(loadBuffer)
};

// --- Persistence ---
via::esp32::NVSStorage nvs;

// --- BLE ---
via::esp32::BleKeyboardHID bleHid;
via::esp32::BLEViaTransport bleVia;

via::Protocol protocol(protocolConfig, bleVia, &nvs);

static uint16_t activeCodes[ROWS * COLS] = {};
static via::KeyboardCallbacks keyboardCallbacks;

via::Keyboard keyboard({ROWS, COLS}, matrix, protocol, bleHid,
                       activeCodes, &keyboardCallbacks);
static bool ready = false;
static WroomOTA ota;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("[VIA] boot"));
    ota.begin([]() {
      if (ready) bleHid.send(via::KeyboardReport{});
    });
    if (!matrix.begin()) {
      Serial.println(F("[VIA] FAIL matrix.begin"));
      return;
    }
    if (!nvs.begin()) {
      Serial.println(F("[VIA] FAIL nvs.begin"));
      return;
    }
    Serial.printf("[VIA] nvs capacity=%u\r\n", (unsigned)nvs.capacity());
    NimBLEDevice::init("AirVIA WROOM32");
    NimBLEDevice::setDeviceName("AirVIA WROOM32");
    NimBLEDevice::setSecurityAuth(true, false, true);
    Serial.println(F("[VIA] nimble init"));
    NimBLEServer* server = NimBLEDevice::createServer();
    if (!server) {
      Serial.println(F("[VIA] FAIL createServer"));
      return;
    }
    Serial.println(F("[VIA] shared server created"));
    if (!bleHid.begin(server, "AirVIA WROOM32", "AirVIA")) {
      Serial.println(F("[VIA] FAIL hid.begin"));
      return;
    }
    Serial.println(F("[VIA] hid service initialized"));
    if (!bleVia.begin(server, "AirVIA WROOM32", 0x00000001)) {
      Serial.println(F("[VIA] FAIL via.begin"));
      return;
    }
    Serial.println(F("[VIA] ff60/ff61/ff62 service initialized"));
    server->advertiseOnDisconnect(true);
    bool started = server->start();
    Serial.printf("[VIA] server started=%d\r\n", started ? 1 : 0);
    if (!started) return;
    NimBLEAdvertising* adv = server->getAdvertising();
    if (!adv) {
      Serial.println(F("[VIA] FAIL getAdvertising"));
      return;
    }
    adv->addServiceUUID(NimBLEUUID((uint16_t)0x1812));
    adv->addServiceUUID(NimBLEUUID(
        "0000FF60-0000-1000-8000-00805F9B34FB"));
    adv->setAppearance(0x03C1);  // HID keyboard
    adv->enableScanResponse(true);
    adv->setName("AirVIA WROOM32");
    adv->start();
    Serial.println(F("[VIA] advertising started"));
    if (!protocol.begin(millis())) {
      Serial.println(F("[VIA] FAIL protocol.begin"));
      return;
    }
    Serial.println(F("[VIA] protocol begin success"));
    if (!keyboard.begin()) {
      Serial.println(F("[VIA] FAIL keyboard.begin"));
      return;
    }
    Serial.println(F("[VIA] keyboard begin success"));
    Serial.println(F("[VIA] ready"));

    pinMode(kEncoderClockPin, INPUT_PULLUP);
    pinMode(kEncoderDetentPin, INPUT_PULLUP);
    ready = true;
}

void loop() {
    ota.task();
    if (!ready) return;
    const uint32_t now = millis();
    protocol.task(now);
    matrix.task(now);
    keyboard.task(now);

    encoder.update(digitalRead(kEncoderClockPin) != LOW,
                   digitalRead(kEncoderDetentPin) != LOW, now);
    int32_t encDelta = encoder.consume();
    (void)encDelta; // ponytail: encoder injection deferred to event pipeline
}
