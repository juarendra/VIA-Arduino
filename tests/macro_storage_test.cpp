#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "VIA_Memory.h"

class CustomState : public via::CustomValue {
 public:
  uint8_t value = 7;
  bool valid = true;
  bool set(uint8_t*) override { return false; }
  bool get(uint8_t*) override { return false; }
  size_t stateSize() const override { return 1; }
  bool saveState(uint8_t* output, size_t size) const override {
    if (size != 1) return false;
    output[0] = value; return true;
  }
  bool validateState(const uint8_t* input, size_t size) const override {
    return valid && size == 1 && input[0] <= 127;
  }
  bool loadState(const uint8_t* input, size_t size) override {
    if (!validateState(input,size)) return false;
    value = input[0]; return true;
  }
};
int main() {
  uint8_t bytes[4096];
  via::MemoryStorage storage(bytes,sizeof(bytes));
  via::MemoryTransport transport;
  const uint16_t defaults[2] = {4,5}, encoderDefaults[2] = {6,7};
  uint16_t oldKeys[2] = {}, oldEncoders[2] = {};
  uint8_t oldStage[13];
  CustomState custom;
  via::Config oldConfig(1,2,1,oldKeys,defaults,nullptr,0,0,1,50,0x80000042UL,
                        1,oldEncoders,encoderDefaults,oldStage,sizeof(oldStage));
  via::Protocol oldProtocol(oldConfig,transport,&storage,&custom);
  assert(oldProtocol.begin());
  assert(oldProtocol.setKeycode(0,0,0,0x1234));
  assert(oldProtocol.setKeycode(0,0,1,0x0056));
  assert(oldProtocol.setEncoderKeycode(0,0,0,0x7701));
  custom.value=42; assert(oldProtocol.save());
  uint8_t legacy[4096]; memcpy(legacy,bytes,sizeof(bytes));

  uint16_t keys[2] = {0x9999,0x9999}, encoders[2] = {0x8888,0x8888};
  uint8_t macros[512], stage[525]; memset(macros,0xCC,sizeof(macros));
  via::Config config(1,2,1,keys,defaults,macros,sizeof(macros),16,1,50,0,
                     1,encoders,encoderDefaults,stage,sizeof(stage));
  custom.value=99;
  via::Protocol protocol(config,transport,&storage,&custom);
  const uint32_t initialRevision=protocol.macroRevision();
  bytes[12]^=1;  // Corrupt a legacy payload, not its CRC.
  assert(!protocol.load());
  assert(keys[0]==0x9999 && encoders[0]==0x8888 && macros[0]==0xCC && custom.value==99);
  assert(protocol.macroRevision()==initialRevision);
  memcpy(bytes,legacy,sizeof(bytes)); bytes[6]^=1;  // Unknown legacy payload size.
  assert(!protocol.load() && keys[0]==0x9999 && macros[0]==0xCC);
  memcpy(bytes,legacy,sizeof(bytes));
  custom.valid=false;
  assert(!protocol.load() && keys[0]==0x9999 && encoders[0]==0x8888 && custom.value==99);
  custom.valid=true;
  assert(protocol.begin(10));
  assert(keys[0]==0x1234 && keys[1]==0x56 && encoders[0]==0x7701);
  assert(protocol.layoutOptions()==0x80000042UL && custom.value==42);
  for (uint8_t byte : macros) assert(byte==0);
  assert(protocol.dirty() && protocol.macroRevision()!=initialRevision);
  assert(!memcmp(bytes,legacy,sizeof(bytes)));  // Migration is staged, not an early erase.

  uint32_t revision=protocol.macroRevision();
  uint8_t write[32]={0x0F,0,0,2,'a',0};
  assert(protocol.process(write,10));
  assert(macros[0]=='a' && protocol.macroRevision()!=revision);
  revision=protocol.macroRevision();
  uint8_t emptyWrite[32]={0x0F};
  assert(protocol.process(emptyWrite,10) && protocol.macroRevision()==revision);
  protocol.task(59); assert(protocol.dirty());
  protocol.task(60); assert(!protocol.dirty());
  keys[0]=0xDEAD; encoders[0]=0xBEEF; macros[0]='z'; custom.value=99;
  assert(protocol.load());
  assert(keys[0]==0x1234 && encoders[0]==0x7701 && macros[0]=='a' && custom.value==42);
  assert(protocol.layoutOptions()==0x80000042UL && !protocol.dirty());
  assert(protocol.macroRevision()!=revision);
  revision=protocol.macroRevision();
  uint8_t reset[32]={0x10}; assert(protocol.process(reset,100));
  assert(macros[0]==0 && protocol.macroRevision()!=revision && keys[0]==0x1234);
  revision=protocol.macroRevision();
  assert(protocol.factoryReset());
  assert(protocol.macroRevision()!=revision && keys[0]==defaults[0] && macros[0]==0);
  puts("MACRO_STORAGE_PASS: CRC-checked migration retains keymap/encoder/bit31/custom, autosave/reload, nonmutating rejection, revision invalidation");
}
