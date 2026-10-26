#include "VIA_Keyboard.h"
#include "VIA_Matrix.h"
#include "VIA_Protocol.h"
#include "VIA_Keycodes.h"
#include "VIA_Macro.h"
#include <string.h>

namespace via {
namespace {
bool containsUsage(const KeyboardReport& report, uint8_t usage) {
  if (usage >= 0xE0 && usage <= 0xE7)
    return (report.modifiers & (1U << (usage - 0xE0))) != 0;
  for (uint8_t key : report.keys) if (key == usage) return true;
  return false;
}

bool changeUsage(KeyboardReport& report, uint8_t usage, bool down) {
  if (usage >= 0xE0 && usage <= 0xE7) {
    const uint8_t mask = 1U << (usage - 0xE0);
    if (down) report.modifiers |= mask;
    else report.modifiers &= static_cast<uint8_t>(~mask);
    return true;
  }
  for (uint8_t& key : report.keys) {
    if (key == usage) {
      if (!down) key = 0;
      return true;
    }
  }
  if (!down) return true;
  for (uint8_t& key : report.keys) if (!key) { key = usage; return true; }
  return false;
}

bool mergeMacro(KeyboardReport& physical, const KeyboardReport& macro) {
  physical.modifiers |= macro.modifiers;
  for (uint8_t key : macro.keys) {
    if (key && (physical.keys[0] == 1 || !changeUsage(physical, key, true))) return false;
  }
  return true;
}
}  // namespace

bool Keyboard::playMacro(uint8_t slot) {
  const uint8_t* data = protocol_.macros();
  const uint16_t size = protocol_.macroBytes();
  if (macroActive_ || !hid_.configured() || hid_.suspended() || slot >= 128 ||
      slot >= protocol_.macroCount() || !data || !size || data[size - 1]) return false;
  uint16_t start = 0;
  for (uint8_t index = 0; index < slot; ++index) {
    while (start < size && data[start]) ++start;
    if (start == size) return false;
    ++start;
  }
  uint16_t end = start;
  while (end < size && data[end]) ++end;
  if (end == size || start == end) return false;
  KeyboardReport held = {};
  uint16_t offset = start;
  while (offset < end) {
    macro_detail::Token token;
    if (!macro_detail::readToken(data, end, offset, token)) return false;
    if (token.action == macro_detail::kDelay) continue;
    if (token.action == macro_detail::kTap) {
      KeyboardReport tap = held;
      if (containsUsage(held, token.usage) || !changeUsage(tap, token.usage, true)) return false;
    } else if (!changeUsage(held, token.usage, token.action == macro_detail::kDown)) {
      return false;
    }
  }
  macroOffset_ = start;
  macroEnd_ = end;
  macroRevision_ = protocol_.macroRevision();
  macroHeld_ = {};
  macroReport_ = {};
  macroPhase_ = kNext;
  macroActive_ = true;
  return true;
}

void Keyboard::cancelMacro() {
  macroActive_ = false;
  macroHeld_ = {};
  macroReport_ = {};
  if (!reportQueued_) reportPending_ = false;
  forceReport_ = true;
}

void Keyboard::advanceMacro(uint32_t now, const KeyboardReport& physical) {
  if (!macroActive_ || reportPending_ || forceReport_) return;
  KeyboardReport acknowledged = physical;
  if (!mergeMacro(acknowledged, macroReport_)) { cancelMacro(); return; }
  if (memcmp(&acknowledged, &lastAcceptedReport_, sizeof(acknowledged))) return;
  if (macroPhase_ == kTapPressed) {
    macroReport_ = macroHeld_;
    macroPhase_ = kTapReleased;
    return;
  }
  if (macroPhase_ == kTapReleased || macroPhase_ == kHeldChanged) {
    macroPhase_ = kNext;
    return;
  }
  if (macroPhase_ == kDelay) {
    if (static_cast<int32_t>(now - macroDue_) >= 0) macroPhase_ = kNext;
    return;
  }
  if (macroPhase_ == kFinishing) {
    macroActive_ = false;
    return;
  }
  if (macroOffset_ == macroEnd_) {
    macroHeld_ = {};
    macroReport_ = {};
    macroPhase_ = kFinishing;
    return;
  }
  macro_detail::Token token;
  uint16_t offset = macroOffset_;
  if (!macro_detail::readToken(protocol_.macros(), macroEnd_, offset, token)) {
    cancelMacro();
    return;
  }
  if (token.action == macro_detail::kDelay) {
    macroDue_ = now + token.delay;
    macroPhase_ = kDelay;
  } else {
    KeyboardReport next = macroHeld_;
    if (token.action == macro_detail::kTap &&
        (containsUsage(physical, token.usage) || physical.keys[0] == 1)) return;
    next.modifiers |= token.modifiers;
    if (!changeUsage(next, token.usage, token.action != macro_detail::kUp)) {
      cancelMacro();
      return;
    }
    KeyboardReport combined = physical;
    if (!mergeMacro(combined, next)) return;
    macroReport_ = next;
    if (token.action == macro_detail::kTap) macroPhase_ = kTapPressed;
    else { macroHeld_ = next; macroPhase_ = kHeldChanged; }
  }
  macroOffset_ = offset;
}

Keyboard::Keyboard(const KeyboardConfig& config, Matrix& matrix,
                   Protocol& protocol, KeyboardHID& hid,
                   uint16_t* activeCodes, KeyboardCallbacks* callbacks)
    : config_(config), matrix_(matrix), protocol_(protocol), hid_(hid),
      activeCodes_(activeCodes), callbacks_(callbacks) {
  layerState_.begin(protocol.layers());
}

bool Keyboard::begin() {
  return config_.rows > 0 && config_.columns > 0 && activeCodes_ != nullptr;
}

uint32_t Keyboard::stableRow(uint8_t row) const {
  return matrix_.stableRow(row);
}

void Keyboard::task(uint32_t now) {
  if (macroActive_ && (macroRevision_ != protocol_.macroRevision() ||
                      !hid_.configured() || hid_.suspended())) cancelMacro();
  if (!hid_.configured() && forceReport_) {
    reportPending_ = false;
    reportQueued_ = false;
  }
  uint8_t leds;
  while (hid_.takeHostLeds(leds)) {
    if (callbacks_ && leds != lastHostLeds_) {
      lastHostLeds_ = leds;
      callbacks_->hostLedsChanged(leds);
    }
  }

  if (reportPending_ && reportQueued_ && hid_.sendComplete()) {
    lastAcceptedReport_ = pendingReport_;
    reportPending_ = false;
    reportQueued_ = false;
  }

  uint32_t cRows = matrix_.changedRows();
  bool resumeTransition = wasSuspended_ && !hid_.suspended();
  if (cRows || resumeTransition) reportNeedsRefresh_ = true;

  if (!cRows && !resumeTransition && !macroActive_ && !reportPending_ &&
      !forceReport_ && !reportNeedsRefresh_) {
    wasSuspended_ = hid_.suspended();
    return;
  }

  uint32_t savedChanged[8];
  if (cRows) {
    for (uint8_t r = 0; r < config_.rows; ++r) savedChanged[r] = matrix_.changedRow(r);
    matrix_.hasChanged();
  }

  const uint8_t rows = config_.rows;
  const uint8_t cols = config_.columns;

  bool bootPending = false;

  if (cRows) {
    for (uint8_t r = 0; r < rows; ++r) {
      if (!(cRows & (1UL << r))) continue;
      uint32_t changed = savedChanged[r];
      uint32_t stable  = matrix_.stableRow(r);
      uint32_t released = changed & ~stable;

      for (uint8_t c = 0; c < cols; ++c) {
        if (!(released & (1UL << c))) continue;
        uint8_t pos = r * cols + c;
        uint16_t code = activeCodes_[pos];
        if (code == 0x0000) continue;

        uint8_t action, layer;
        if (extractLayerAction(code, action, layer))
          layerState_.applyLayerRelease(0, code);

        activeCodes_[pos] = 0x0000;
      }
    }

    for (uint8_t r = 0; r < rows; ++r) {
      if (!(cRows & (1UL << r))) continue;
      uint32_t changed = savedChanged[r];
      uint32_t stable  = matrix_.stableRow(r);
      uint32_t pressed  = changed & stable;

      for (uint8_t c = 0; c < cols; ++c) {
        if (!(pressed & (1UL << c))) continue;
        uint8_t pos = r * cols + c;
        uint16_t code = layerState_.resolve(layerState_.defaultLayer(),
                                            protocol_.keymap(), r, c, rows, cols);

        uint8_t action, layer;
        if (extractLayerAction(code, action, layer)) {
          layerState_.applyLayerPress(0, code);
          activeCodes_[pos] = code;
        } else if (classifyKeycode(code) == KeycodeType::kBoot) {
          activeCodes_[pos] = code;
          bootPending = true;
        }
      }
    }

    for (uint8_t r = 0; r < rows; ++r) {
      if (!(cRows & (1UL << r))) continue;
      uint32_t changed = savedChanged[r];
      uint32_t stable  = matrix_.stableRow(r);
      uint32_t pressed  = changed & stable;

      for (uint8_t c = 0; c < cols; ++c) {
        if (!(pressed & (1UL << c))) continue;
        uint8_t pos = r * cols + c;
        if (activeCodes_[pos] != 0x0000) continue;

        uint16_t code = layerState_.resolve(layerState_.defaultLayer(),
                                            protocol_.keymap(), r, c, rows, cols);

        KeycodeType type = classifyKeycode(code);
        if (type == KeycodeType::kMacro) {
          activeCodes_[pos] = code;
          playMacro(static_cast<uint8_t>(code & 0x7F));
          continue;
        }
        if (type == KeycodeType::kLayer || type == KeycodeType::kBoot) {
          if (type == KeycodeType::kBoot) bootPending = true;
          activeCodes_[pos] = code;
          continue;
        }
        if (type == KeycodeType::kNone || type == KeycodeType::kTransparent) {
          activeCodes_[pos] = 0x0000;
          continue;
        }

        activeCodes_[pos] = code;
      }
    }
  }

  if (hid_.suspended()) {
    wasSuspended_ = true;
    if (hid_.remoteWakeupAllowed() && !wakeRequested_) {
      const uint8_t total = rows * cols;
      for (uint8_t i = 0; i < total; ++i) {
        if (activeCodes_[i] != 0x0000) {
          hid_.remoteWakeup();
          wakeRequested_ = true;
          break;
        }
      }
    }
    return;
  }

  wasSuspended_ = false;
  wakeRequested_ = false;

  const KeyboardReport physical = buildReport();
  advanceMacro(now, physical);
  KeyboardReport desired = physical;
  if (!mergeMacro(desired, macroReport_)) {
    cancelMacro();
    desired = physical;
  }
  if (!reportPending_ || !reportQueued_) reportNeedsRefresh_ = false;
  if (!reportPending_ &&
      (forceReport_ || memcmp(&desired, &lastAcceptedReport_, sizeof(desired)))) {
    pendingReport_ = desired;
    reportPending_ = true;
    forceReport_ = false;
  } else if (reportPending_ && !reportQueued_) {
    pendingReport_ = desired;
  }
  if (reportPending_ && !reportQueued_ && hid_.configured())
    reportQueued_ = hid_.send(pendingReport_);

  if (bootPending && callbacks_)
    callbacks_->bootloaderRequested();
}

KeyboardReport Keyboard::buildReport() const {
  KeyboardReport r = {0, 0, {0, 0, 0, 0, 0, 0}};
  uint8_t basicCount = 0;
  const uint8_t total = config_.rows * config_.columns;

  // First pass: count basic keys and collect modifiers
  for (uint8_t i = 0; i < total; ++i) {
    uint16_t code = activeCodes_[i];
    if (code == 0x0000) continue;

    KeycodeType type = classifyKeycode(code);
    if (type == KeycodeType::kBasic) basicCount++;
    else if (type == KeycodeType::kModifier) r.modifiers |= extractModifierMask(code);
  }

  if (basicCount > 6) {
    for (int i = 0; i < 6; ++i) r.keys[i] = 0x01; // ErrorRollOver
    return r;
  }

  // Second pass: fill keys with deduplication
  uint8_t ki = 0;
  for (uint8_t i = 0; i < total && ki < 6; ++i) {
    uint16_t code = activeCodes_[i];
    if (code == 0x0000) continue;
    if (classifyKeycode(code) != KeycodeType::kBasic) continue;

    uint8_t usage = extractBasicUsage(code);
    bool dup = false;
    for (uint8_t k = 0; k < ki; ++k) {
      if (r.keys[k] == usage) { dup = true; break; }
    }
    if (!dup) r.keys[ki++] = usage;
  }

  return r;
}

}  // namespace via
