#include <assert.h>
#include "VIA_Keycodes.h"

int main() {
  assert(via::classifyKeycode(0x0000) == via::KeycodeType::kNone);
  assert(via::classifyKeycode(0x0001) == via::KeycodeType::kTransparent);
  assert(via::classifyKeycode(0x0004) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x00A4) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x00E0) == via::KeycodeType::kModifier);
  assert(via::classifyKeycode(0x00E7) == via::KeycodeType::kModifier);
  assert(via::classifyKeycode(0x0100) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x1FFF) == via::KeycodeType::kBasic);
  assert(via::classifyKeycode(0x5220) == via::KeycodeType::kLayer);
  assert(via::classifyKeycode(0x5260) == via::KeycodeType::kLayer);
  assert(via::classifyKeycode(0x7C00) == via::KeycodeType::kBoot);
  assert(via::classifyKeycode(0x2000) == via::KeycodeType::kUnsupported);
  assert(via::classifyKeycode(0x8000) == via::KeycodeType::kUnsupported);

  assert(via::extractBasicUsage(0x0004) == 0x04);
  assert(via::extractBasicUsage(0x00A4) == 0xA4);

  assert(via::extractModifierMask(0x00E0) == (1U << 0));
  assert(via::extractModifierMask(0x00E1) == (1U << 1));
  assert(via::extractModifierMask(0x00E6) == (1U << 6));
  assert(via::extractModifierMask(0x00E7) == (1U << 7));

  {
    uint8_t usage = 0xFF, mod = 0xFF;
    assert(via::extractQkMods(0x0104, usage, mod));
    assert(usage == 0x04);
    assert(mod == 0x01);
  }
  {
    uint8_t usage = 0xFF, mod = 0xFF;
    assert(via::extractQkMods(0x0204, usage, mod));
    assert(usage == 0x04);
    assert(mod == 0x02);
  }
  {
    uint8_t usage = 0xFF, mod = 0xFF;
    assert(via::extractQkMods(0x1F04, usage, mod));
    assert(usage == 0x04);
    assert(mod == 0xF0);
  }
  {
    uint8_t usage = 0xFF, mod = 0xFF;
    assert(via::extractQkMods(0x1204, usage, mod));
    assert(usage == 0x04);
    assert(mod == 0x20);
  }
  {
    uint8_t usage = 0xFF, mod = 0xFF;
    assert(!via::extractQkMods(0x0004, usage, mod));
  }

  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(via::extractLayerAction(0x5200, action, layer));
    assert(action == 0);
    assert(layer == 0);
  }
  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(via::extractLayerAction(0x5203, action, layer));
    assert(action == 0);
    assert(layer == 3);
  }
  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(via::extractLayerAction(0x5221, action, layer));
    assert(action == 1);
    assert(layer == 1);
  }
  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(via::extractLayerAction(0x5240, action, layer));
    assert(action == 2);
    assert(layer == 0);
  }
  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(via::extractLayerAction(0x526E, action, layer));
    assert(action == 3);
    assert(layer == 0x0E);
  }
  {
    uint8_t action = 0xFF, layer = 0xFF;
    assert(!via::extractLayerAction(0x004, action, layer));
  }

  return 0;
}
