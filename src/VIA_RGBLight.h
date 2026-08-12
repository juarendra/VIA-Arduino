#pragma once

#include "VIA_Protocol.h"

namespace via {

struct RGBLightState {
  uint8_t brightness;
  uint8_t effect;
  uint8_t speed;
  uint8_t hue;
  uint8_t saturation;
};

class RGBLightCallbacks {
 public:
  virtual ~RGBLightCallbacks() {}
  virtual void apply(const RGBLightState& state) = 0;
};

/* VIA/QMK custom-value channel 2 (qmk_rgblight). Rendering WS2812, APA102,
 * etc. remains board-specific and is delegated to RGBLightCallbacks. */
class RGBLight : public CustomValue {
 public:
  RGBLight(RGBLightState& state, RGBLightCallbacks* callbacks = nullptr)
      : state_(state), callbacks_(callbacks) {}
  bool set(uint8_t packet[kPacketSize]) override;
  bool get(uint8_t packet[kPacketSize]) override;
  bool save(uint8_t packet[kPacketSize]) override;
  size_t stateSize() const override { return sizeof(RGBLightState); }
  bool saveState(uint8_t* output, size_t length) const override;
  bool loadState(const uint8_t* input, size_t length) override;

 private:
  void apply();
  RGBLightState& state_;
  RGBLightCallbacks* callbacks_;
};

}  // namespace via
