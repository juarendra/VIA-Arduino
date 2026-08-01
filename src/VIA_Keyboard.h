#pragma once

#include <stdint.h>
#include "VIA_Keycodes.h"

namespace via {

struct KeyboardReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
};

class KeyboardCallbacks {
 public:
  virtual void hostLedsChanged(uint8_t /*leds*/) {}
  virtual void bootloaderRequested() {}
 protected:
  ~KeyboardCallbacks() = default;
};

class LayerState {
 public:
  void begin(uint8_t layerCount) { layerCount_ = layerCount; }
  uint8_t layerCount() const { return layerCount_; }

  void applyLayerPress(uint8_t /*sourceLayer*/, uint16_t keycode) {
    uint8_t action, layer;
    if (!extractLayerAction(keycode, action, layer)) return;
    if (layer >= layerCount_) return;
    switch (action) {
      case 0:
        refCounts_[layer]++;
        transientMask_ |= (1UL << layer);
        break;
      case 1:
        transientMask_ ^= (1UL << layer);
        break;
      case 2:
        transientMask_ = (1UL << layer);
        for (uint8_t i = 0; i < layerCount_; ++i) refCounts_[i] = 0;
        break;
      case 3:
        defaultLayer_ = layer;
        break;
    }
  }

  void applyLayerRelease(uint8_t /*sourceLayer*/, uint16_t keycode) {
    uint8_t action, layer;
    if (!extractLayerAction(keycode, action, layer)) return;
    if (layer >= layerCount_) return;
    if (action == 0) {
      if (refCounts_[layer] > 0) {
        refCounts_[layer]--;
        if (refCounts_[layer] == 0)
          transientMask_ &= ~(1UL << layer);
      }
    }
  }

  uint16_t resolve(uint8_t defaultLayer, const uint16_t* keymap,
                   uint8_t row, uint8_t col, uint8_t rows, uint8_t cols) const {
    uint32_t mask = transientMask_;
    for (int8_t i = 31; i >= 0; --i) {
      if (mask & (1UL << i) && i < layerCount_) {
        uint16_t code = keymap[i * rows * cols + row * cols + col];
        if (code == 0x0000) return code;
        if (code != 0x0001) return code;
      }
    }
    if (defaultLayer < layerCount_)
      return keymap[defaultLayer * rows * cols + row * cols + col];
    return 0x0000;
  }

  uint32_t activeLayerMask() const { return transientMask_; }
  uint8_t defaultLayer() const { return defaultLayer_; }

 private:
  uint32_t transientMask_ = 0;
  uint8_t refCounts_[32] = {};
  uint8_t layerCount_ = 0;
  uint8_t defaultLayer_ = 0;
};

class KeyboardHID {
 public:
  virtual bool configured() const = 0;
  virtual bool send(const KeyboardReport& r) = 0;
  virtual bool sendComplete() = 0;
  virtual bool takeHostLeds(uint8_t& leds) = 0;
  virtual bool suspended() const = 0;
  virtual bool remoteWakeupAllowed() const = 0;
  virtual bool remoteWakeup() = 0;

 protected:
  ~KeyboardHID() = default;
};

class Protocol;
class Matrix;

struct KeyboardConfig {
  uint8_t rows;
  uint8_t columns;
};

class Keyboard {
 public:
  Keyboard(const KeyboardConfig& config, Matrix& matrix, Protocol& protocol,
           KeyboardHID& hid, uint16_t* activeCodes,
           KeyboardCallbacks* callbacks = nullptr);
  bool begin();
  void task(uint32_t now);
  uint32_t stableRow(uint8_t row) const;

 private:
  KeyboardReport buildReport() const;

  const KeyboardConfig config_;
  Matrix& matrix_;
  Protocol& protocol_;
  KeyboardHID& hid_;
  uint16_t* activeCodes_;
  KeyboardCallbacks* callbacks_;
  LayerState layerState_;
  KeyboardReport pendingReport_ = {};
  bool reportPending_ = false;
  KeyboardReport lastAcceptedReport_ = {};
  uint8_t lastHostLeds_ = 0;
  bool wakeRequested_ = false;
  bool wasSuspended_ = false;
};

}  // namespace via
