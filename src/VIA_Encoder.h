#pragma once

#include <stdint.h>

namespace via {

class Encoder {
 public:
  constexpr static uint16_t kDebounceDefault = 2000;

  void setDebounceUs(uint16_t us) { debounceUs_ = us; }

  void update(uint8_t a, uint8_t b, uint32_t now) {
    uint8_t state = (a ? 2 : 0) | (b ? 1 : 0);
    if (state == lastState_) return;
    uint32_t elapsed = now - lastChangeTs_;
    if (elapsed < debounceUs_) return;
    lastState_ = state;
    lastChangeTs_ = now;

    // ponytail: 4-state quadrature, quarter-step per edge, CCW = negative
    switch (lastPinState_) {
      case 0:
        if (state == 1) steps_--;   // 00→01: CCW
        if (state == 2) steps_++;   // 00→10: CW
        break;
      case 1:
        if (state == 3) steps_--;   // 01→11: CCW
        if (state == 0) steps_++;   // 01→00: CW
        break;
      case 2:
        if (state == 0) steps_--;   // 10→00: CCW
        if (state == 3) steps_++;   // 10→11: CW
        break;
      case 3:
        if (state == 2) steps_--;   // 11→10: CCW
        if (state == 1) steps_++;   // 11→01: CW
        break;
    }
    lastPinState_ = state;
  }

  int32_t count() const { return steps_ / 4; }

  int32_t consume() {
    int32_t c = count();
    steps_ = 0;
    return c;
  }

 private:
  uint8_t lastState_ = 0;
  uint8_t lastPinState_ = 0;
  uint32_t lastChangeTs_ = 0;
  uint16_t debounceUs_ = 0;
  int32_t steps_ = 0;
};

}  // namespace via
