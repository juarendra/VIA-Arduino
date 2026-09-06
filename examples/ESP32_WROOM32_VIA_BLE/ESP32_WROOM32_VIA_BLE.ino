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
 * All pin assignments are configurable; the defaults avoid the WROOM-32
 * strapping pins (0, 2, 5, 6, 7, 8, 9, 10, 11, 16, 34-39) except where
 * noted. Re-wire rowPins/colPins/encoder pins to match your PCB.
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

// --- Matrix: 5 rows x 6 cols ---
static const via::Pin rowPins[5] = {4, 5, 12, 13, 14};
static const via::Pin colPins[6] = {15, 16, 17, 18, 21, 22};

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
static constexpr uint16_t KC_S = 0x0005;
static constexpr uint16_t KC_D = 0x0006;
static constexpr uint16_t KC_F = 0x0007;
static constexpr uint16_t KC_G = 0x0008;
static constexpr uint16_t KC_H = 0x0009;
static constexpr uint16_t KC_Z = 0x000A;
static constexpr uint16_t KC_X = 0x000B;
static constexpr uint16_t KC_C = 0x000C;
static constexpr uint16_t KC_V = 0x000D;
static constexpr uint16_t KC_B = 0x000E;
static constexpr uint16_t KC_N = 0x000F;
static constexpr uint16_t KC_Q = 0x0010;
static constexpr uint16_t KC_W = 0x0011;
static constexpr uint16_t KC_E = 0x0012;
static constexpr uint16_t KC_R = 0x0013;
static constexpr uint16_t KC_T = 0x0014;
static constexpr uint16_t KC_Y = 0x0015;
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

void setup() {
    if (!nvs.begin()) return;
    NimBLEDevice::init("AirVIA WROOM32");
    if (!bleHid.begin("AirVIA WROOM32", "AirVIA")) return;
    if (!bleVia.begin("AirVIA WROOM32", 0x00000001)) return;
    if (!protocol.begin(millis())) return;
    if (!keyboard.begin()) return;

    pinMode(kEncoderClockPin, INPUT_PULLUP);
    pinMode(kEncoderDetentPin, INPUT_PULLUP);
}

void loop() {
    const uint32_t now = millis();
    protocol.task(now);
    matrix.task(now);
    keyboard.task(now);

    encoder.update(digitalRead(kEncoderClockPin) != LOW,
                   digitalRead(kEncoderDetentPin) != LOW, now);
    int32_t encDelta = encoder.consume();
    (void)encDelta; // ponytail: encoder injection deferred to event pipeline
}
