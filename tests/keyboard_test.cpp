#include <assert.h>
#include "VIA_Keycodes.h"
#include "VIA_Keyboard.h"

class FakeKeyboardHID : public via::KeyboardHID {
 public:
  bool _configured = true, _suspended = false, _wakeAllowed = false;
  bool sendResult = true, completeResult = true;
  uint32_t sendCalls = 0, completeCalls = 0, wakeCalls = 0;
  via::KeyboardReport lastReport;
  uint8_t hostLeds = 0;
  bool configured() const override { return _configured; }
  bool send(const via::KeyboardReport& r) override { sendCalls++; lastReport = r; return sendResult; }
  bool sendComplete() override { completeCalls++; return completeResult; }
  bool takeHostLeds(uint8_t& l) override { l = hostLeds; hostLeds = 0; return true; }
  bool suspended() const override { return _suspended; }
  bool remoteWakeupAllowed() const override { return _wakeAllowed; }
  bool remoteWakeup() override { wakeCalls++; return true; }
};

int main() {
  {
    FakeKeyboardHID hid;
    via::KeyboardReport r = {0x01, 0, {0x04, 0, 0, 0, 0, 0}};
    assert(hid.send(r));
    assert(hid.lastReport.modifiers == 0x01);
    assert(hid.lastReport.keys[0] == 0x04);
    assert(hid.sendCalls == 1);
    assert(hid.configured());
    assert(!hid.suspended());
    assert(!hid.remoteWakeupAllowed());
  }

  {
    FakeKeyboardHID hid;
    uint8_t leds = 0xFF;
    assert(hid.takeHostLeds(leds));
    assert(leds == 0);
  }

  {
    FakeKeyboardHID hid;
    hid.hostLeds = 0x02;
    uint8_t leds = 0;
    assert(hid.takeHostLeds(leds));
    assert(leds == 0x02);
    assert(hid.hostLeds == 0);
  }

  {
    FakeKeyboardHID hid;
    assert(hid.remoteWakeup());
    assert(hid.wakeCalls == 1);
  }

  {
    FakeKeyboardHID hid;
    hid.sendResult = false;
    assert(!hid.send(via::KeyboardReport{}));
  }

  {
    FakeKeyboardHID hid;
    hid.completeResult = false;
    assert(!hid.sendComplete());
  }

  {
    FakeKeyboardHID hid;
    hid._suspended = true;
    assert(hid.suspended());
  }

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

  // --- LayerState tests ---

  // Helper: build keycode for layer action
  auto mkLayer = [](uint8_t action, uint8_t layer) -> uint16_t {
    return 0x5200 | ((action & 0x03) << 5) | (layer & 0x1F);
  };

  // MO press/release
  {
    via::LayerState ls;
    ls.begin(2);
    uint16_t km[8] = {   // 2 layers, 2 rows, 2 cols
      0x0004, 0x0005,    // layer 0: A, B
      0x0006, 0x0007,    //          C, D
      0x0008, 0x0009,    // layer 1: E, F
      0x000A, 0x000B     //          G, H
    };
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0004); // default layer 0
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1)
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0008); // layer 1 E at (0,0)
    ls.applyLayerRelease(1, mkLayer(0, 1));
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0004); // back to layer 0
  }

  // TG toggle
  {
    via::LayerState ls;
    ls.begin(2);
    uint16_t km[8] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0008, 0x0009,
      0x000A, 0x000B
    };
    ls.applyLayerPress(1, mkLayer(1, 1)); // TG(1) press
    ls.applyLayerRelease(1, mkLayer(1, 1)); // TG release: no-op
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0008); // still layer 1
    ls.applyLayerPress(1, mkLayer(1, 1)); // TG(1) press again
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0004); // toggled off
  }

  // TO clears transients, activates one
  {
    via::LayerState ls;
    ls.begin(3);
    uint16_t km[12] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0008, 0x0009,
      0x000A, 0x000B,
      0x000C, 0x000D,
      0x000E, 0x000F
    };
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1)
    ls.applyLayerPress(2, mkLayer(2, 2)); // TO(2)
    assert((ls.activeLayerMask() & (1U << 1)) == 0); // layer 1 cleared
    assert((ls.activeLayerMask() & (1U << 2)) != 0); // layer 2 active
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x000C); // layer 2 key at (0,0)
  }

  // DF changes default
  {
    via::LayerState ls;
    ls.begin(2);
    ls.applyLayerPress(1, mkLayer(3, 1)); // DF(1)
    assert(ls.defaultLayer() == 1);
  }

  // Duplicate MO: ref count prevents premature removal
  {
    via::LayerState ls;
    ls.begin(2);
    uint16_t km[8] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0008, 0x0009,
      0x000A, 0x000B
    };
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1) first press
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1) second press (duplicate)
    ls.applyLayerRelease(1, mkLayer(0, 1)); // release one
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0008); // still layer 1
    ls.applyLayerRelease(1, mkLayer(0, 1)); // release second
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0004); // back to layer 0
  }

  // Highest active layer wins
  {
    via::LayerState ls;
    ls.begin(3);
    // layer 0: A, layer 1: KC_TRNS, layer 2: X
    uint16_t km[12] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0001, 0x0005,    // layer 1 (0,0)=TRNS
      0x0006, 0x0007,
      0x0008, 0x0005,    // layer 2 (0,0)=E
      0x0006, 0x0007
    };
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1)
    ls.applyLayerPress(2, mkLayer(0, 2)); // MO(2)
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0008); // highest=layer 2
  }

  // KC_TRNS falls through
  {
    via::LayerState ls;
    ls.begin(2);
    uint16_t km[8] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0001, 0x0005,    // layer 1 (0,0)=TRNS
      0x0006, 0x0007
    };
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1)
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0004); // falls through to layer 0
  }

  // KC_NO stops lookup
  {
    via::LayerState ls;
    ls.begin(2);
    uint16_t km[8] = {
      0x0004, 0x0005,
      0x0006, 0x0007,
      0x0000, 0x0005,    // layer 1 (0,0)=NO
      0x0006, 0x0007
    };
    ls.applyLayerPress(1, mkLayer(0, 1)); // MO(1)
    assert(ls.resolve(0, km, 0, 0, 2, 2) == 0x0000); // KC_NO stops
  }

  // Invalid layer target: no-op
  {
    via::LayerState ls;
    ls.begin(2);
    ls.applyLayerPress(5, mkLayer(0, 5)); // layer 5 invalid
    assert(ls.activeLayerMask() == 0); // no change
    ls.applyLayerPress(0, mkLayer(1, 0)); // TG(0) invalid (layer 0 always default)
    // TG(0) is technically weird — but layer >= layerCount check catches 0 which IS valid
    // Testing >= layerCount
  }

  return 0;
}
