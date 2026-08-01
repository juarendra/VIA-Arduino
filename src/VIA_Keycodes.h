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
  (void)code;
  return KeycodeType::kUnsupported;
}

inline uint8_t extractBasicUsage(uint16_t code) {
  (void)code;
  return 0;
}

inline uint8_t extractModifierMask(uint16_t code) {
  (void)code;
  return 0;
}

inline bool extractQkMods(uint16_t code, uint8_t& hidUsage, uint8_t& modMask) {
  (void)code;
  (void)hidUsage;
  (void)modMask;
  return false;
}

inline bool extractLayerAction(uint16_t code, uint8_t& action, uint8_t& layer) {
  (void)code;
  (void)action;
  (void)layer;
  return false;
}

}  // namespace via
