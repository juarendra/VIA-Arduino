#pragma once

#include <stdint.h>

namespace via {

enum class KeycodeType {
  kNone,
  kTransparent,
  kBasic,
  kModifier,
  kLayer,
  kBoot,
  kUnsupported
};

inline KeycodeType classifyKeycode(uint16_t code) {
  switch (code >> 8) {
    case 0x00:
      if (code == 0x0000) return KeycodeType::kNone;
      if (code == 0x0001) return KeycodeType::kTransparent;
      if (code >= 0x0004 && code <= 0x00A4) return KeycodeType::kBasic;
      if (code >= 0x00E0 && code <= 0x00E7) return KeycodeType::kModifier;
      return KeycodeType::kUnsupported;
    case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x05: case 0x06: case 0x07: case 0x08:
    case 0x09: case 0x0A: case 0x0B: case 0x0C:
    case 0x0D: case 0x0E: case 0x0F: case 0x10:
    case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x15: case 0x16: case 0x17: case 0x18:
    case 0x19: case 0x1A: case 0x1B: case 0x1C:
    case 0x1D: case 0x1E: case 0x1F:
      return KeycodeType::kBasic;
    case 0x52:
      return KeycodeType::kLayer;
    case 0x7C:
      return KeycodeType::kBoot;
    default:
      return KeycodeType::kUnsupported;
  }
}

inline uint8_t extractBasicUsage(uint16_t code) {
  return code & 0xFF;
}

inline uint8_t extractModifierMask(uint16_t code) {
  return 1U << (code - 0xE0);
}

inline bool extractQkMods(uint16_t code, uint8_t& hidUsage, uint8_t& modMask) {
  if (code < 0x0100 || code > 0x1FFF) return false;
  hidUsage = code & 0xFF;
  modMask = (code >> 8) & 0x0F;
  if (code & 0x1000) modMask <<= 4;
  return true;
}

inline bool extractLayerAction(uint16_t code, uint8_t& action, uint8_t& layer) {
  if ((code & 0xFF00) != 0x5200) return false;
  action = (code >> 5) & 0x03;
  layer = code & 0x1F;
  return true;
}

}  // namespace via
