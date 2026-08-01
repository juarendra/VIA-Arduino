/*
 * Raspberry Pi Pico / RP2040 reference for VIA_Arduino.
 *
 * Board package: Raspberry Pi Pico/RP2040 by Earle F. Philhower, III
 * Tools -> USB Stack: Adafruit TinyUSB
 * Library dependency: Adafruit TinyUSB Library
 *
 * This example exposes the required dedicated VIA Raw HID interface and
 * persistent keymap/macro storage. Add your own keyboard HID interface and
 * key scanning code alongside it for a complete keyboard product.
 */

#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>  // Ensure Arduino CLI discovers the RP2040 EEPROM library.
#include <VIA_Arduino.h>
#include <VIA_EEPROMStorage.h>
#include <VIA_TinyUSB_Keyboard.h>
#include <VIA_TinyUSB_RawHID.h>

static uint16_t keymap[2 * 2 * 3] = {
    0x0004, 0x0005, 0x0006,  // Layer 0: A B C
    0x0007, 0x0008, 0x0009,  // Layer 0: D E F
    0x0014, 0x001A, 0x001B,  // Layer 1: Q W X
    0x001D, 0x001C, 0x0018,  // Layer 1: Z Y U
};
static const uint16_t defaultKeymap[2 * 2 * 3] = {
    0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009,
    0x0014, 0x001A, 0x001B, 0x001D, 0x001C, 0x0018,
};
static uint8_t macros[512];
static uint8_t loadBuffer[sizeof(keymap) + sizeof(macros) + sizeof(uint32_t)];

via::tinyusb::RawHID rawHid;
via::tinyusb::Keyboard usbKeyboard;
via::EEPROMStorage storage(1024);
via::Config config = {
    2, 3, 2, keymap, defaultKeymap, macros, sizeof(macros), 16, 1, 750,
    0, 0, nullptr, nullptr, loadBuffer, sizeof(loadBuffer),
};
via::Protocol keyboard(config, rawHid, &storage);

void setup() {
  storage.begin();
  usbKeyboard.begin("VIA Keyboard");
  rawHid.begin("VIA Raw HID");
  keyboard.begin(millis());
}

void loop() {
  keyboard.task(millis());

  // Scan your physical matrix here. For each key event, get its assigned
  // QMK/VIA keycode with keyboard.keycode(activeLayer, row, column), then send
  // its boot-keyboard report through usbKeyboard. Add mouse and consumer HID
  // interfaces when those features are required.
}
