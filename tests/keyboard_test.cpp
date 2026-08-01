#include <assert.h>
#include <string.h>
#include "VIA_Matrix.h"
#include "VIA_Keycodes.h"
#include "VIA_Protocol.h"
#include "VIA_Keyboard.h"

class FakeMatrixIO : public via::MatrixIO {
 public:
  FakeMatrixIO() : inputPullupCalls(0), driveLowCalls(0), releaseCalls(0),
                    readCalls(0), delayMicrosecondsCalls(0), readPos(0), drivenIdx(0) {
    for (uint8_t i = 0; i < sizeof(keyMap); ++i) keyMap[i] = 0;
  }
  void inputPullup(via::Pin) override { ++inputPullupCalls; }
  void driveLow(via::Pin pin) override { ++driveLowCalls; drivenIdx = pin & 0x1F; readPos = 0; }
  void release(via::Pin) override { ++releaseCalls; }
  bool read(via::Pin) override { ++readCalls; return !(keyMap[drivenIdx] & (1U << readPos++)); }
  void delayMicroseconds(uint16_t) override { ++delayMicrosecondsCalls; }
  uint8_t inputPullupCalls, driveLowCalls, releaseCalls, readCalls, delayMicrosecondsCalls;
  uint8_t keyMap[32];
 private:
  uint8_t readPos, drivenIdx;
};

class FakeKeyboardHID : public via::KeyboardHID {
 public:
  bool _configured = true, _suspended = false, _wakeAllowed = false;
  bool sendResult = true, completeResult = true;
  uint32_t sendCalls = 0, completeCalls = 0, wakeCalls = 0;
  via::KeyboardReport lastReport;
  uint8_t hostLeds = 0;
  bool hasLeds_ = false;
  bool configured() const override { return _configured; }
  bool send(const via::KeyboardReport& r) override { sendCalls++; lastReport = r; return sendResult; }
  bool sendComplete() override { completeCalls++; return completeResult; }
  bool takeHostLeds(uint8_t& l) override { l = hostLeds; bool had = hasLeds_; hostLeds = 0; hasLeds_ = false; return had; }
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
    hid.hasLeds_ = true;
    uint8_t leds = 0xFF;
    assert(hid.takeHostLeds(leds));
    assert(leds == 0);
    assert(!hid.hasLeds_);
  }

  {
    FakeKeyboardHID hid;
    hid.hostLeds = 0x02;
    hid.hasLeds_ = true;
    uint8_t leds = 0;
    assert(hid.takeHostLeds(leds));
    assert(leds == 0x02);
    assert(hid.hostLeds == 0);
    assert(!hid.hasLeds_);
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

  // --- Keyboard integration tests ---

  class NullTransport : public via::Transport {
   public:
    bool receive(uint8_t[via::kPacketSize]) override { return false; }
    bool send(const uint8_t[via::kPacketSize]) override { return true; }
  };
  NullTransport transport;

  // Helper: setup keymap 2 rows x 3 cols, 1 layer. row0: A B C, row1: D E F.
  const uint8_t kRows = 2, kCols = 3;
  const uint16_t defaultKm[6] = {0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009};

  // Test 1: basic press/release — activeCodes + HID report
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, defaultKm,
                              nullptr, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                              nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[6] = {0};

    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 1); // press row0 col1 (B)
    matrix.task(0);
    keyboard.task(0);
    assert(ac[1] == 0x0005);
    assert(hid.sendCalls == 1);
    assert(hid.lastReport.keys[0] == 0x05);
    assert(hid.lastReport.modifiers == 0);
    for (int i = 1; i < 6; ++i) assert(hid.lastReport.keys[i] == 0);

    io.keyMap[100 & 0x1F] = 0; // release
    matrix.task(100);
    keyboard.task(100);
    assert(ac[1] == 0x0000);
    assert(hid.sendCalls == 2);
    assert(hid.lastReport.keys[0] == 0);
    assert(hid.lastReport.modifiers == 0);
  }

  // Test 2: modifier key press/release
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    km[0] = 0x00E0; // left control
    via::Config pc(kRows, kCols, 1, km, km,
                              nullptr, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                              nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[6] = {0};

    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0);
    matrix.task(0);
    keyboard.task(0);
    assert(ac[0] == 0x00E0);
    assert(hid.sendCalls == 1);
    assert(hid.lastReport.modifiers == 0x01);
    assert(hid.lastReport.keys[0] == 0);

    io.keyMap[100 & 0x1F] = 0;
    matrix.task(100);
    keyboard.task(100);
    assert(ac[0] == 0x0000);
    assert(hid.sendCalls == 2);
    assert(hid.lastReport.modifiers == 0);
  }

  // Test 3: VIA remap while held — activeCodes holds original, release sends original
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, defaultKm,
                              nullptr, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                              nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[6] = {0};

    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0); // press A (0x04)
    matrix.task(0);
    keyboard.task(0);
    assert(ac[0] == 0x0004);
    assert(hid.lastReport.keys[0] == 0x04);

    proto.setKeycode(0, 0, 0, 0x0005); // remap to B while held
    assert(ac[0] == 0x0004); // activeCodes still holds original

    io.keyMap[100 & 0x1F] = 0; // release
    matrix.task(100);
    keyboard.task(100);
    assert(ac[0] == 0x0000);
    assert(hid.lastReport.keys[0] == 0); // empty report (original usage removed)
    assert(hid.lastReport.modifiers == 0);
  }

  // Test 4: layer MO — layer press processed first, remaining key resolves on new layer
  {
    uint16_t km[12]; // 2 layers, 2x3
    km[0 * kCols + 0] = 0x5201; // layer 0 (0,0) = MO(1)
    km[0 * kCols + 1] = 0x0005; // layer 0 (0,1) = B
    km[0 * kCols + 2] = 0x0006; // layer 0 (0,2) = C
    km[1 * kCols + 0] = 0x0007; // layer 0 (1,0) = D
    km[1 * kCols + 1] = 0x0008; // layer 0 (1,1) = E
    km[1 * kCols + 2] = 0x0009; // layer 0 (1,2) = F
    km[6 + 0] = 0x000B;         // layer 1 (0,0) = K
    km[6 + 1] = 0x000C;         // layer 1 (0,1) = L
    km[6 + 2] = 0x000D;         // layer 1 (0,2) = M
    km[6 + 3] = 0x000E;         // layer 1 (1,0) = N
    km[6 + 4] = 0x000F;         // layer 1 (1,1) = O
    km[6 + 5] = 0x0010;         // layer 1 (1,2) = P

    via::Config pc(kRows, kCols, 2, km, km,
                              nullptr, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                              nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[6] = {0};

    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0) | (1U << 1); // press col0 (MO) + col1 (B)
    matrix.task(0);
    keyboard.task(0);
    assert((ac[0] & 0xFF00) == 0x5200); // stored as layer action
    assert(ac[1] == 0x000C);            // resolved on layer 1 (0,1) = L
    assert(hid.lastReport.keys[0] == 0x0C);

    io.keyMap[100 & 0x1F] = 0; // release both
    matrix.task(100);
    keyboard.task(100);
    assert(ac[0] == 0x0000);
    assert(ac[1] == 0x0000);
    assert(hid.lastReport.keys[0] == 0);
    assert(hid.lastReport.modifiers == 0);
  }

  // Test 5: six-key rollover — 7 keys produce ErrorRollOver
  {
    const uint8_t R = 2, C = 6;
    uint16_t km[12];
    for (int i = 0; i < 12; ++i) km[i] = 0x0004 + i;
    via::Config pc(R, C, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                             0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[6] = {200, 201, 202, 203, 204, 205};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {R, C, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[12] = {0};

    via::Keyboard keyboard(via::KeyboardConfig{R, C}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = 0x3F; // 6 keys on row0
    io.keyMap[101 & 0x1F] = 0x01; // 1 key on row1 = 7 total
    matrix.task(0);
    keyboard.task(0);
    for (int i = 0; i < 6; ++i) assert(hid.lastReport.keys[i] == 0x01);
  }

  // --- Task 10: Coalesce, Retry, LED, Suspend ---

  // Busy send: hid.sendResult=false then true; report sent exactly once with latest wanted state
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                   0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    hid.sendResult = false;
    hid.completeResult = false;
    uint16_t ac[6] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0); // press A
    matrix.task(0);
    keyboard.task(0);
    assert(ac[0] == 0x0004);
    assert(hid.sendCalls == 1); // tried to send, failed
    assert(hid.lastReport.keys[0] == 0x04);

    // No change on next scan — still in flight, retry send
    matrix.task(10);
    keyboard.task(10);
    assert(hid.sendCalls == 2); // retried once
    assert(hid.lastReport.keys[0] == 0x04);

    // Press B while A still pending, send still failing
    io.keyMap[100 & 0x1F] |= (1U << 1); // press B alongside A
    hid.sendResult = false;
    matrix.task(20);
    keyboard.task(20);
    assert(ac[1] == 0x0005); // B now active
    // Should rebuild because state changed (B added), coalesce with A
    assert(hid.lastReport.keys[0] == 0x04);
    assert(hid.lastReport.keys[1] == 0x05);

    // Now succeed — send returns true, complete returns true
    hid.sendResult = true;
    hid.completeResult = true;
    matrix.task(30);
    keyboard.task(30);
    // After sendComplete=true, report accepted. No changes → no more sends.
    int callsAfterAccept = hid.sendCalls;
    matrix.task(40);
    keyboard.task(40);
    assert(hid.sendCalls == callsAfterAccept);
  }

  // sendComplete() false while in-flight: task blocks next scan cycle
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                   0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    hid.completeResult = false; // transfer still in flight
    uint16_t ac[6] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0);
    matrix.task(0);
    keyboard.task(0);
    assert(ac[0] == 0x0004);
    uint32_t sendCount = hid.sendCalls;

    // In-flight — task should skip reprocessing, retry send without rebuilding
    matrix.task(10);
    keyboard.task(10);
    assert(hid.sendCalls == sendCount + 1); // one retry

    // Now complete, same report — no new send
    hid.completeResult = true;
    matrix.task(20);
    keyboard.task(20);
    uint32_t afterAccept = hid.sendCalls;
    matrix.task(30);
    keyboard.task(30);
    assert(hid.sendCalls == afterAccept); // no extra send, report already accepted
  }

  // Host LED: inject 0x03 (Num+Caps), assert callback receives 0x03
  {
    class LEDCallback : public via::KeyboardCallbacks {
     public:
      uint8_t lastLeds = 0xFF; // sentinel
      int ledCalls = 0;
      void hostLedsChanged(uint8_t leds) override { lastLeds = leds; ledCalls++; }
    };
    LEDCallback cb;

    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                   0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    uint16_t ac[6] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac, &cb);
    assert(keyboard.begin());

    // No matrix changes, but LEDs arrive
    hid.hostLeds = 0x03;
    hid.hasLeds_ = true;
    keyboard.task(0);
    assert(cb.ledCalls == 1);
    assert(cb.lastLeds == 0x03);

    // Same LED value — no callback fired again
    hid.hostLeds = 0x03;
    hid.hasLeds_ = true;
    keyboard.task(10);
    assert(cb.ledCalls == 1);

    // LED change
    hid.hostLeds = 0x01;
    hid.hasLeds_ = true;
    keyboard.task(20);
    assert(cb.ledCalls == 2);
    assert(cb.lastLeds == 0x01);
  }

  // Suspend: press key, no send, wake requested once; second press no duplicate wake
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                   0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    hid._suspended = true;
    hid._wakeAllowed = true;
    uint16_t ac[6] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    io.keyMap[100 & 0x1F] = (1U << 0); // press A while suspended
    matrix.task(0);
    keyboard.task(0);
    assert(ac[0] == 0x0004);
    assert(hid.sendCalls == 0);       // no send while suspended
    assert(hid.wakeCalls == 1);       // wake requested once

    // Second press while still suspended — no duplicate wake
    io.keyMap[100 & 0x1F] = (1U << 1); // press B
    matrix.task(10);
    keyboard.task(10);
    assert(hid.wakeCalls == 1); // still 1

    // Release — no send, no extra wake
    io.keyMap[100 & 0x1F] = 0;
    matrix.task(20);
    keyboard.task(20);
    assert(hid.sendCalls == 0);
    assert(hid.wakeCalls == 1);
  }

  // Resume: clear suspend, assert report is rebuilt and sent
  {
    uint16_t km[6]; memcpy(km, defaultKm, sizeof(km));
    via::Config pc(kRows, kCols, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                   0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    via::Pin rp[2] = {100, 101}, cp[3] = {200, 201, 202};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    via::MatrixConfig mc = {kRows, kCols, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    FakeKeyboardHID hid;
    hid._suspended = true;
    hid._wakeAllowed = true;
    uint16_t ac[6] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, kCols}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    // Press key while suspended
    io.keyMap[100 & 0x1F] = (1U << 0);
    matrix.task(0);
    keyboard.task(0);
    assert(hid.sendCalls == 0);
    assert(hid.wakeCalls == 1);

    // Clear suspend — host wakes us
    hid._suspended = false;
    // Force a matrix change so task processes
    io.keyMap[100 & 0x1F] = (1U << 0); // still held
    matrix.task(10);
    keyboard.task(10);
    // After resume, report should be rebuilt and sent
    assert(hid.sendCalls == 1);
    assert(hid.lastReport.keys[0] == 0x04);
    assert(hid.wakeCalls == 1); // no new wake
  }

  // Test 6: stableRow passthrough
  {
    via::Pin rp[2] = {100, 101}, cp[2] = {200, 201};
    uint32_t raw[2] = {0,0}, cnd[2] = {0,0}, stb[2] = {0,0}, chg[2] = {0,0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x02;
    via::MatrixConfig mc = {kRows, 2, rp, cp, via::kColToRow, 30, 0,
                            raw, cnd, stb, chg};
    via::Matrix matrix(mc, io);
    assert(matrix.begin());

    uint16_t km[4] = {0x0004, 0x0005, 0x0006, 0x0007};
    via::Config pc(kRows, 2, 1, km, km, nullptr, 0, 0, 0, 0, 0,
                             0, nullptr, nullptr, nullptr, 0);
    via::Protocol proto(pc, transport);
    assert(proto.begin(0));

    FakeKeyboardHID hid;
    uint16_t ac[4] = {0};
    via::Keyboard keyboard(via::KeyboardConfig{kRows, 2}, matrix, proto, hid, ac);
    assert(keyboard.begin());

    matrix.task(0);
    assert(keyboard.stableRow(0) == 0x02);
    assert(keyboard.stableRow(1) == 0);
  }

  return 0;
}
