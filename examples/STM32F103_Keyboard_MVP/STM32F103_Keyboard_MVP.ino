/*
 * STM32F103 Keyboard MVP reference for VIA_Arduino.
 *
 * Board: custom STM32F103CBT6, 8 MHz HSE
 * Core: STMicroelectronics:stm32@3.0.0
 * Upload: SWD during development
 *
 * 6 rows x 18 columns x 4 layers, COL2ROW, active-low
 * This example compiles but is NOT hardware-verified.
 */

#include <Arduino.h>
#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_STM32F1_GPIO.h>
#include <VIA_STM32F1_USB.h>
#include <VIA_STM32F1_Boot.h>

// --- Matrix ---
static const via::Pin rowPins[6] = {PB10, PB11, PB12, PB13, PB14, PB15};
static const via::Pin colPins[18] = {
    PA0,  PA1,  PA2,  PA3,  PA4,  PA5,  PA6,  PA7,
    PA8,  PA9,  PA10, PA15, PB0,  PB1,  PB3,  PB4,
    PB5,  PB6
};

static uint32_t rawRows[6]       = {};
static uint32_t candidateRows[6] = {};
static uint32_t stableRows[6]    = {};
static uint32_t changedRows[6]   = {};

via::stm32f1::MatrixIOArduino matrixIO;

via::MatrixConfig matrixConfig = {
    6, 18, rowPins, colPins, via::kColToRow, 30, 5,
    rawRows, candidateRows, stableRows, changedRows
};

via::Matrix matrix(matrixConfig, matrixIO);

// --- Keymap ---
static uint16_t keymap[6 * 18 * 4] = {};
static const uint16_t defaultKeymap[6 * 18 * 4] = {};

via::Config protocolConfig = {
    6, 18, 4, keymap, defaultKeymap,
    nullptr, 0, 0, 1, 750
};

// --- USB ---
via::stm32f1::UsbDevice usb;

// --- Protocol ---
via::Protocol protocol(protocolConfig, usb);

// --- Active Codes ---
static uint16_t activeCodes[6 * 18] = {};

// --- Keyboard ---
via::KeyboardCallbacks keyboardCallbacks;

via::Keyboard keyboard(matrix, protocol, usb, keyboardCallbacks,
                       activeCodes, 6, 18);

// --- Boot ---
via::stm32f1::BootCoordinator boot(protocol, usb, keyboardCallbacks);

// --- Setup ---
void setup() {
  usb.begin();
  protocol.begin(millis());
  keyboard.begin();
}

// --- Loop ---
void loop() {
  usb.task();
  protocol.task(millis());
  keyboard.task(millis());
  boot.task();
}
