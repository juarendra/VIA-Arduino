#include "VIA_Keyboard.h"
#include "VIA_Matrix.h"
#include "VIA_Protocol.h"
#include "VIA_Keycodes.h"
#include <string.h>

namespace via {

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

void Keyboard::task(uint32_t /*now*/) {
  uint8_t leds;
  while (hid_.takeHostLeds(leds)) {
    if (callbacks_ && leds != lastHostLeds_) {
      lastHostLeds_ = leds;
      callbacks_->hostLedsChanged(leds);
    }
  }

  if (reportPending_) {
    if (hid_.sendComplete()) {
      lastAcceptedReport_ = pendingReport_;
      reportPending_ = false;
    } else {
      hid_.send(pendingReport_);
    }
  }

  uint32_t cRows = matrix_.changedRows();
  bool resumeTransition = wasSuspended_ && !hid_.suspended();

  if (!cRows && !resumeTransition) {
    wasSuspended_ = hid_.suspended();
    return;
  }

  uint32_t savedChanged[8];
  for (uint8_t r = 0; r < config_.rows; ++r)
    savedChanged[r] = matrix_.changedRow(r);
  matrix_.hasChanged();

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

  KeyboardReport desired = buildReport();

  if (reportPending_) {
    if (memcmp(&desired, &pendingReport_, sizeof(desired)) != 0) {
      pendingReport_ = desired;
      hid_.send(desired);
    }
  } else {
    if (memcmp(&desired, &lastAcceptedReport_, sizeof(desired)) != 0) {
      pendingReport_ = desired;
      reportPending_ = true;
      hid_.send(desired);
    }
  }

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
