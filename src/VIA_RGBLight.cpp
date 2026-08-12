#include "VIA_RGBLight.h"

#include <string.h>

namespace via {

bool RGBLight::set(uint8_t packet[kPacketSize]) {
  const bool v3Form = packet[1] >= 0x80;
  if (!v3Form && packet[1] != 0x02) return false;
  const uint8_t command = v3Form ? packet[1] : packet[2];
  
  switch (command) {
    case 0x80:
    case 0x01: state_.brightness = packet[v3Form ? 2 : 3]; break;
    case 0x81:
    case 0x02: state_.effect = packet[v3Form ? 2 : 3]; break;
    case 0x82:
    case 0x03: state_.speed = packet[v3Form ? 2 : 3]; break;
    case 0x83:
    case 0x04:
      state_.hue = packet[v3Form ? 2 : 3];
      state_.saturation = packet[v3Form ? 3 : 4];
      break;
    default: return false;
  }
  apply();
  return true;
}

bool RGBLight::get(uint8_t packet[kPacketSize]) {
  const bool v3Form = packet[1] >= 0x80;
  if (!v3Form && packet[1] != 0x02) return false;
  const uint8_t command = v3Form ? packet[1] : packet[2];

  switch (command) {
    case 0x80:
    case 0x01: packet[v3Form ? 2 : 3] = state_.brightness; break;
    case 0x81:
    case 0x02: packet[v3Form ? 2 : 3] = state_.effect; break;
    case 0x82:
    case 0x03: packet[v3Form ? 2 : 3] = state_.speed; break;
    case 0x83:
    case 0x04:
      packet[v3Form ? 2 : 3] = state_.hue;
      packet[v3Form ? 3 : 4] = state_.saturation;
      break;
    default: return false;
  }
  return true;
}

bool RGBLight::save(uint8_t packet[kPacketSize]) {
  const bool v3Form = packet[1] >= 0x80;
  return v3Form || packet[1] == 0x02;
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
