#pragma once
#include <stdint.h>

namespace via {
namespace macro_detail {
enum Action { kTap = 1, kDown = 2, kUp = 3, kDelay = 4 };
struct Token {
  Action action;
  uint8_t usage;
  uint8_t modifiers;
  uint16_t delay;
};

inline bool asciiUsage(uint8_t character, uint8_t& usage, uint8_t& modifiers) {
  modifiers = 0;
  if (character >= 'a' && character <= 'z') usage = 4 + character - 'a';
  else if (character >= 'A' && character <= 'Z') {
    usage = 4 + character - 'A'; modifiers = 2;
  } else if (character >= '1' && character <= '9') usage = 0x1E + character - '1';
  else if (character == '0') usage = 0x27;
  else {
    switch (character) {
      case ' ': usage = 0x2C; return true;
      case '\t': usage = 0x2B; return true;
      case '\n': usage = 0x28; return true;
      case '\b': usage = 0x2A; return true;
      case 27: usage = 0x29; return true;
      case 127: usage = 0x4C; return true;
      default: break;
    }
    static const char plain[] = "-=[]\\;'`,./";
    static const char shifted[] = "_+{}|:\"~<>?";
    static const uint8_t usages[] = {0x2D,0x2E,0x2F,0x30,0x31,0x33,0x34,0x35,0x36,0x37,0x38};
    for (uint8_t i = 0; i < sizeof(usages); ++i) {
      if (character == plain[i] || character == shifted[i]) {
        usage = usages[i]; modifiers = character == shifted[i] ? 2 : 0; return true;
      }
    }
    static const char digits[] = "!@#$%^&*()";
    for (uint8_t i = 0; i < 10; ++i) {
      if (character == digits[i]) { usage = i == 9 ? 0x27 : 0x1E + i; modifiers = 2; return true; }
    }
    return false;
  }
  return true;
}

inline bool readToken(const uint8_t* data, uint16_t end, uint16_t& offset, Token& token) {
  if (offset >= end) return false;
  token = {kTap, 0, 0, 0};
  const uint8_t character = data[offset++];
  if (character != 1) return asciiUsage(character, token.usage, token.modifiers);
  if (offset >= end) return false;
  const uint8_t action = data[offset++];
  if (action >= kTap && action <= kUp) {
    if (offset >= end) return false;
    token.action = static_cast<Action>(action);
    token.usage = data[offset++];
    return (token.usage >= 4 && token.usage <= 0xA4) ||
           (token.usage >= 0xE0 && token.usage <= 0xE7);
  }
  if (action != kDelay) return false;
  token.action = kDelay;
  uint32_t milliseconds = 0;
  const uint16_t start = offset;
  while (offset < end && data[offset] >= '0' && data[offset] <= '9') {
    milliseconds = milliseconds * 10 + data[offset++] - '0';
    if (milliseconds > 65535) return false;
  }
  if (offset == start || offset >= end || data[offset++] != '|') return false;
  token.delay = static_cast<uint16_t>(milliseconds);
  return true;
}
}  // namespace macro_detail
}  // namespace via
