#include "VIA_RGBLight.h"

#include <string.h>

namespace via {

bool RGBLight::set(uint8_t packet[kPacketSize]) {
  if (packet[1] != 0x02) return false;
  switch (packet[2]) {
    case 0x01: state_.brightness = packet[3]; break;
    case 0x02: state_.effect = packet[3]; break;
    case 0x03: state_.speed = packet[3]; break;
    case 0x04:
      state_.hue = packet[3];
      state_.saturation = packet[4];
      break;
    default: return false;
  }
  apply();
  return true;
}

bool RGBLight::get(uint8_t packet[kPacketSize]) {
  if (packet[1] != 0x02) return false;
  switch (packet[2]) {
    case 0x01: packet[3] = state_.brightness; break;
    case 0x02: packet[3] = state_.effect; break;
    case 0x03: packet[3] = state_.speed; break;
    case 0x04:
      packet[3] = state_.hue;
      packet[4] = state_.saturation;
      break;
    default: return false;
  }
  return true;
}

bool RGBLight::saveState(uint8_t* output, size_t length) const {
  if (length != sizeof(state_)) return false;
  memcpy(output, &state_, sizeof(state_));
  return true;
}

bool RGBLight::loadState(const uint8_t* input, size_t length) {
  if (length != sizeof(state_)) return false;
  memcpy(&state_, input, sizeof(state_));
  apply();
  return true;
}

void RGBLight::apply() {
  if (callbacks_) callbacks_->apply(state_);
}

}  // namespace via
