#include "VIA_Keyboard.h"
#include "VIA_Matrix.h"
#include "VIA_Protocol.h"
#include "VIA_Keycodes.h"

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
  uint32_t cRows = matrix_.changedRows();
  if (!cRows) return;
  
  // ponytail: save changes before clearing, hasChanged zeros everything
  uint32_t savedChanged[8];
  for (uint8_t r = 0; r < config_.rows; ++r)
    savedChanged[r] = matrix_.changedRow(r);
  matrix_.hasChanged();

  const uint8_t rows = config_.rows;
  const uint8_t cols = config_.columns;

  // Step 1: process releases
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

  // Step 2: process layer-action presses
  bool bootPending = false;
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

  // Step 3: process remaining (non-layer, non-boot) presses
  for (uint8_t r = 0; r < rows; ++r) {
    if (!(cRows & (1UL << r))) continue;
    uint32_t changed = savedChanged[r];
    uint32_t stable  = matrix_.stableRow(r);
    uint32_t pressed  = changed & stable;

    for (uint8_t c = 0; c < cols; ++c) {
      if (!(pressed & (1UL << c))) continue;
      uint8_t pos = r * cols + c;
      if (activeCodes_[pos] != 0x0000) continue; // already handled in step 2

      uint16_t code = layerState_.resolve(layerState_.defaultLayer(),
                                          protocol_.keymap(), r, c, rows, cols);

      KeycodeType type = classifyKeycode(code);
      if (type == KeycodeType::kLayer || type == KeycodeType::kBoot) {
        // layer actions and boot handled in step 2; boot shouldn't appear
        // unless it was the first press and step 2 caught it. Fall through just in case.
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

  buildAndSend();

  if (bootPending && callbacks_)
    callbacks_->bootloaderRequested();
}

void Keyboard::buildAndSend() {
  KeyboardReport r = buildReport();
  hid_.send(r);
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
