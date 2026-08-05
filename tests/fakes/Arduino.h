#pragma once
#include <stdint.h>
#include <map>

#define INPUT_PULLUP 1
#define OUTPUT 2
#define LOW 0
#define HIGH 1
#define D0 0
#define D1 1

struct FakeArduino {
  static std::map<uint8_t, int> mode;
  static std::map<uint8_t, int> value;
};
std::map<uint8_t, int> FakeArduino::mode;
std::map<uint8_t, int> FakeArduino::value;

void pinMode(uint8_t pin, int m) { FakeArduino::mode[pin] = m; }
void digitalWrite(uint8_t pin, int val) { FakeArduino::value[pin] = val; }
int digitalRead(uint8_t pin) { return FakeArduino::value[pin]; }
void delayMicroseconds(uint16_t) {}
