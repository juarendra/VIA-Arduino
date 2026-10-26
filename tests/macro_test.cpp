#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "VIA_Keyboard.h"
#include "VIA_Matrix.h"
#include "VIA_Memory.h"

namespace {
class Pins : public via::MatrixIO {
 public:
  uint8_t pressed = 0;
  bool read(via::Pin pin) override { return !(pressed & (1U << pin)); }
};
class Reports : public via::KeyboardHID {
 public:
  bool connected = true, suspended_ = false, accepted = true, complete = true;
  via::KeyboardReport history[256] = {};
  unsigned count = 0;
  bool configured() const override { return connected; }
  bool send(const via::KeyboardReport& report) override {
    if (!connected || !accepted) return false;
    assert(count < 256);
    history[count++] = report;
    return true;
  }
  bool sendComplete() override { return complete; }
  bool takeHostLeds(uint8_t&) override { return false; }
  bool suspended() const override { return suspended_; }
  bool remoteWakeupAllowed() const override { return false; }
  bool remoteWakeup() override { return false; }
};
class Bench {
 public:
  uint16_t defaults[8] = {0x7700,4,5,6,7,8,9,10}, keys[8] = {}, active[8] = {};
  uint8_t macros[128] = {};
  via::MemoryTransport transport;
  via::Config config;
  via::Protocol protocol;
  const via::Pin rows[1] = {10}, columns[8] = {0,1,2,3,4,5,6,7};
  uint32_t raw[1] = {}, candidate[1] = {}, stable[1] = {}, changed[1] = {};
  Pins pins;
  via::MatrixConfig matrixConfig;
  via::Matrix matrix;
  Reports hid;
  via::Keyboard keyboard;
  uint32_t now = 0;
  Bench() : config(1,8,1,keys,defaults,macros,sizeof(macros),4),
            protocol(config,transport),
            matrixConfig{1,8,rows,columns,via::kColToRow,0,0,raw,candidate,stable,changed},
            matrix(matrixConfig,pins), keyboard({1,8},matrix,protocol,hid,active) {
    assert(matrix.begin() && protocol.begin() && keyboard.begin());
  }
  void load(const uint8_t* data, unsigned bytes) {
    assert(bytes <= sizeof(macros));
    memset(macros,0,sizeof(macros));
    memcpy(macros,data,bytes);
  }
  void pump(unsigned ticks, uint8_t pressed = 1) {
    pins.pressed = pressed;
    for (unsigned i = 0; i < ticks; ++i) {
      matrix.task(now); keyboard.task(now++);
    }
  }
};
bool has(const via::KeyboardReport& report, uint8_t usage) {
  for (uint8_t key : report.keys) if (key == usage) return true;
  return false;
}
unsigned presses(const Reports& hid, uint8_t usage, uint8_t modifiers = 0) {
  unsigned count = 0;
  for (unsigned i = 0; i < hid.count; ++i)
    if (has(hid.history[i],usage) && hid.history[i].modifiers == modifiers) ++count;
  return count;
}
void assertClean(const Reports& hid) {
  assert(hid.count);
  const via::KeyboardReport empty = {};
  assert(!memcmp(&hid.history[hid.count-1],&empty,sizeof(empty)));
}
void textAndHeldTrigger() {
  Bench b;
  const uint8_t text[] = {'a','a','A','!','\n',0};
  b.load(text,sizeof(text)); b.pump(120);
  assert(!b.keyboard.macroPlaying());
  assert(presses(b.hid,4) == 2 && presses(b.hid,4,2) == 1);
  assert(presses(b.hid,0x1E,2) == 1 && presses(b.hid,0x28) == 1);
  assertClean(b.hid);
  const unsigned reports = b.hid.count;
  b.pump(100); assert(b.hid.count == reports);
}
void rejectedAndUncompletedReports() {
  Bench b;
  const uint8_t text[] = {'a','a',0}; b.load(text,sizeof(text));
  b.hid.accepted = false; b.pump(10);
  assert(b.hid.count == 0 && b.keyboard.macroPlaying());
  b.hid.accepted = true; b.hid.complete = false; b.pump(20);
  assert(b.hid.count == 1 && has(b.hid.history[0],4));
  b.hid.complete = true; b.pump(100);
  assert(presses(b.hid,4) == 2 && !b.keyboard.macroPlaying());
  assert(b.hid.count >= 4 && !has(b.hid.history[1],4));
  assertClean(b.hid);
}
void chordPreservesPhysicalKey() {
  Bench b;
  const uint8_t chord[] = {1,2,0xE0,1,1,6,1,3,0xE0,0}; b.load(chord,sizeof(chord));
  b.pump(5,4); b.hid.count = 0; b.pump(100,5);
  assert(presses(b.hid,6,1) == 1 && !b.keyboard.macroPlaying());
  for (unsigned i=0;i<b.hid.count;++i) assert(has(b.hid.history[i],5));
  assert(b.hid.history[b.hid.count-1].modifiers == 0);
}
void waitsForPhysicalReleaseAndRolloverSpace() {
  Bench b;
  const uint8_t text[] = {'a',0}; b.load(text,sizeof(text));
  b.pump(5,2); b.hid.count = 0; b.pump(20,3);
  assert(b.keyboard.macroPlaying() && b.hid.count == 0);
  b.pump(100,1);
  assert(presses(b.hid,4) == 1 && !has(b.hid.history[0],4));
  assertClean(b.hid);
  Bench full;
  const uint8_t g[] = {'g',0}; full.load(g,sizeof(g));
  full.pump(5,0x7E); full.hid.count=0; full.pump(20,0x7F);
  assert(full.keyboard.macroPlaying() && presses(full.hid,10) == 0);
  full.pump(100,0x7D);
  assert(presses(full.hid,10) == 1 && !full.keyboard.macroPlaying());
  for (unsigned i=0;i<full.hid.count;++i) assert(!has(full.hid.history[i],1));
}
void validatesBeforeTypingAndBoundsSlots() {
  const uint8_t invalid[][12] = {
    {'a',1,1,0}, {'a',0xC3,0xA9,0}, {'a',1,4,'7','0','0','0','0','|',0},
    {1,2,4,1,1,4,0}, {'a',1,9,4,0}
  };
  for (const auto& program : invalid) {
    Bench b; b.load(program,sizeof(program)); b.pump(40);
    assert(!b.keyboard.macroPlaying() && b.hid.count == 0);
  }
  Bench empty;
  assert(!empty.keyboard.playMacro(0) && empty.hid.count == 0);
  Bench unterminated;
  memset(unterminated.macros, 'a', sizeof(unterminated.macros));
  assert(!unterminated.keyboard.playMacro(0) && unterminated.hid.count == 0);
  Bench guarded;
  const uint8_t text[]={'a',0}; guarded.load(text,sizeof(text)); guarded.macros[127]=255;
  guarded.pump(40); assert(!guarded.keyboard.macroPlaying() && guarded.hid.count==0);
  Bench last;
  const uint8_t slots[]={0,0,0,'Z',0}; last.load(slots,sizeof(slots));
  assert(!last.keyboard.playMacro(4));
  assert(last.protocol.setKeycode(0,0,0,0x7703)); last.pump(60);
  assert(presses(last.hid,0x1D,2)==1 && !last.keyboard.macroPlaying());
}
void delayAcrossClockWrap() {
  Bench b;
  const uint8_t text[]={'a',1,4,'2','0','|','b',0}; b.load(text,sizeof(text));
  b.now=0xFFFFFFF0UL; b.pump(20);
  assert(b.keyboard.macroPlaying() && presses(b.hid,5)==0);
  b.pump(80); assert(presses(b.hid,5)==1 && !b.keyboard.macroPlaying());
  assertClean(b.hid);
}
void cancelOnEditDisconnectSuspend() {
  for (unsigned mode=0;mode<4;++mode) {
    Bench b;
    const uint8_t text[]={1,2,0xE0,1,4,'1','0','0','|',1,1,6,0}; b.load(text,sizeof(text));
    b.pump(20); assert(b.keyboard.macroPlaying() && b.hid.history[b.hid.count-1].modifiers==1);
    if (mode==0) {
      uint8_t packet[32]={0x0F,0,0,1,'x'}; assert(b.protocol.process(packet,b.now));
    } else if (mode==1) b.hid.connected=false;
    else if (mode==2) b.hid.suspended_=true;
    else b.keyboard.cancelMacro();
    b.pump(2); assert(!b.keyboard.macroPlaying());
    b.hid.connected=true; b.hid.suspended_=false; b.pump(150);
    assert(presses(b.hid,6,1)==0); assertClean(b.hid);
  }
}
}
int main() {
  textAndHeldTrigger(); rejectedAndUncompletedReports(); chordPreservesPhysicalKey();
  waitsForPhysicalReleaseAndRolloverSpace(); validatesBeforeTypingAndBoundsSlots();
  delayAcrossClockWrap(); cancelOnEditDisconnectSuspend();
  puts("MACRO_TEST_PASS: text/chords, ACK/retry, physical conflicts/6KRO, bounds, wrap, edit/disconnect/suspend/cancel");
}
