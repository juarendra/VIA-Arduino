#include <VIA_Arduino.h>
#include <VIA_Keycodes.h>
#include <VIA_Keyboard.h>
#include <VIA_Matrix.h>
#include <VIA_STM32F1_USB.h>

static via::Pin rowPins[6] = {PB10, PB11, PB12, PB13, PB14, PB15};
static via::Pin colPins[18] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10, PA15, PB0, PB1, PB3, PB4, PB5, PB6};
static uint16_t keymap[6 * 18 * 4];
static const uint16_t defaultKeymap[6 * 18 * 4];
static uint32_t rawRows[6], candidateRows[6], stableRows[6], changedRows[6];
static uint16_t activeCodes[6 * 18];
static uint8_t loadBuffer[2048];

via::stm32f1::UsbDevice usb;
via::MatrixIO matrixIO;
via::MatrixConfig matrixConfig{6, 18, rowPins, colPins, via::kColToRow, 10, 5, rawRows, candidateRows, stableRows, changedRows};
via::Matrix matrix(matrixConfig, matrixIO);
via::Config protocolConfig(6, 18, 4, keymap, defaultKeymap, nullptr, 0, 0, 1, 0, 0, 0, nullptr, nullptr, loadBuffer, sizeof(loadBuffer));
via::Protocol protocol(protocolConfig, usb);
via::KeyboardConfig keyboardConfig{6, 18};
via::Keyboard keyboard(keyboardConfig, matrix, protocol, usb, activeCodes);

void setup() {
  usb.begin();
  protocol.begin(millis());
  keyboard.begin();
}

void loop() {
  usb.task();
  protocol.task(millis());
  keyboard.task(millis());
}
