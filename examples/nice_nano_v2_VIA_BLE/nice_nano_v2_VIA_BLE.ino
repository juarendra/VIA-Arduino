#include <Arduino.h>
#include <bluefruit.h>

#include <VIA.h>

// Pins for row and column matrix
static const uint8_t rowPins[2] = {D0, D1};
static const uint8_t colPins[3] = {D2, D3, D4};

// Required variables for the VIA matrix and keymap
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

static uint16_t keymap[LAYERS][ROWS][COLS] = {
    // Layer 0
    {
        {KC_A, KC_B, KC_C},
        {KC_D, KC_E, MO_1}
    },
    // Layer 1
    {
        {KC_1, KC_2, KC_3},
        {KC_4, KC_5, KC_TRNS}
    }
};

// BLE Services
BLEHidAdafruit bleHid;

// VIA adapters
AirVIA::Adapters::AdafruitNrf52BleKeyboardAdapter bleKeyboard(&bleHid);
AirVIA::Adapters::AdafruitNrf52BleViaAdapter bleVia;
AirVIA::Adapters::EepromStorageAdapter storage(4096);

static uint8_t loadBuffer[sizeof(keymap) + sizeof(uint32_t)] = {};
AirVIA::Protocol protocol(bleVia, storage, loadBuffer, sizeof(loadBuffer));
AirVIA::Keyboard keyboard(protocol, bleKeyboard, rowPins, ROWS, colPins, COLS, keymap[0][0], sizeof(keymap), LAYERS);

void startAdvertising() {
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    
    Bluefruit.Advertising.addService(bleHid);
    Bluefruit.Advertising.addService(bleVia.getService());
    
    Bluefruit.ScanResponse.addName();
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void setup() {
    Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);
    Bluefruit.begin(1, 0);
    Bluefruit.setName("AirVIA nice!nano");
    Bluefruit.setTxPower(4);
    Bluefruit.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    bleHid.begin();
    bleKeyboard.begin();
    bleVia.begin("AirVIA nice!nano", 0x00000001);
    storage.begin();
    protocol.begin(millis());
    keyboard.begin();
    startAdvertising();
}

void loop() {
    uint32_t now = millis();
    protocol.task(now);
    keyboard.task(now);
}
