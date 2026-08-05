#include <Arduino.h>
#include <bluefruit.h>
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_Encoder.h>
#include <VIA_Battery.h>
#include <VIA_SleepMgr.h>
#include <VIA_nRF52_GPIO.h>
#include <VIA_nRF52_InternalFS.h>
#include <VIA_nRF52_BLE.h>
#include <VIA_nRF52_BLE_ViaTransport.h>

// Pins for row and column matrix
static const via::Pin rowPins[2] = {D0, D1};
static const via::Pin colPins[3] = {D2, D3, D4};

static constexpr uint8_t ROWS = 2;
static constexpr uint8_t COLS = 3;
static constexpr uint8_t LAYERS = 2;

// Keycodes (from QMK naming)
static constexpr uint16_t KC_A = 0x0004;
static constexpr uint16_t KC_B = 0x0005;
static constexpr uint16_t KC_C = 0x0006;
static constexpr uint16_t KC_D = 0x0007;
static constexpr uint16_t KC_E = 0x0008;
static constexpr uint16_t KC_1 = 0x001E;
static constexpr uint16_t KC_2 = 0x001F;
static constexpr uint16_t KC_3 = 0x0020;
static constexpr uint16_t KC_4 = 0x0021;
static constexpr uint16_t KC_5 = 0x0022;
static constexpr uint16_t KC_TRNS = 0x0001;
static constexpr uint16_t MO_1 = 0x5221;

static uint32_t rawRows[2]       = {};
static uint32_t candidateRows[2] = {};
static uint32_t stableRows[2]    = {};
static uint32_t changedRows[2]   = {};

via::nrf52::MatrixIOArduino matrixIO;

via::MatrixConfig matrixConfig = {
    ROWS, COLS, rowPins, colPins, via::kColToRow, 30, 5,
    rawRows, candidateRows, stableRows, changedRows
};

via::Matrix matrix(matrixConfig, matrixIO);

static uint16_t keymap[LAYERS * ROWS * COLS] = {};
static uint8_t loadBuffer[sizeof(keymap) + sizeof(uint32_t)] = {};
static uint8_t storageStaging[4096] = {};
static const uint16_t defaultKeymap[LAYERS * ROWS * COLS] = {
    // Layer 0
    KC_A, KC_B, KC_C,
    KC_D, KC_E, MO_1,
    // Layer 1
    KC_1, KC_2, KC_3,
    KC_4, KC_5, KC_TRNS
};

via::Config protocolConfig = {
    ROWS, COLS, LAYERS, keymap, defaultKeymap,
    nullptr, 0, 0, 1, 750, 0, 0, nullptr, nullptr,
    loadBuffer, sizeof(loadBuffer)
};

via::nrf52::InternalFSStorage storage(storageStaging, sizeof(storageStaging));

BLEHidAdafruit bleHidSvc;
via::nrf52::BLEKeyboardHID bleHid(bleHidSvc);
via::nrf52::BLEViaTransport bleVia;
via::Protocol protocol(protocolConfig, bleVia, &storage);

static uint16_t activeCodes[ROWS * COLS] = {};
static via::KeyboardCallbacks keyboardCallbacks;

via::Keyboard keyboard({ROWS, COLS}, matrix, protocol, bleHid,
                       activeCodes, &keyboardCallbacks);

void startAdvertising() {
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    
    Bluefruit.Advertising.addService(bleHidSvc);
    Bluefruit.Advertising.addService(bleVia.service());
    
    Bluefruit.ScanResponse.addName();
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void setup() {
    Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
    if (!Bluefruit.begin(1, 0)) return;
    Bluefruit.setName("AirVIA nice!nano");
    Bluefruit.setTxPower(4);
    Bluefruit.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    Bluefruit.Security.setIOCaps(false, false, false);

    bleHidSvc.begin();
    if (!bleHid.begin()) return;
    if (!bleVia.begin("AirVIA nice!nano", 0x00000001)) return;
    if (!storage.begin()) return;
    if (!protocol.begin(millis())) return;
    if (!keyboard.begin()) return;
    startAdvertising();
}

void loop() {
    const uint32_t now = millis();
    protocol.task(now);
    keyboard.task(now);
}
