#include <assert.h>
#include "VIA_Matrix.h"

namespace {

class FakeMatrixIO : public via::MatrixIO {
 public:
  FakeMatrixIO() : inputPullupCalls(0), driveLowCalls(0), releaseCalls(0),
                   readCalls(0), delayMicrosecondsCalls(0), readValue(false) {}

  void inputPullup(via::Pin pin) override {
    ++inputPullupCalls;
    pinStates[pin] = true;
  }
  void driveLow(via::Pin pin) override {
    ++driveLowCalls;
    pinStates[pin] = false;
  }
  void release(via::Pin pin) override {
    ++releaseCalls;
    pinStates[pin] = true;
  }
  bool read(via::Pin pin) override {
    ++readCalls;
    return pinStates[pin];
  }
  void delayMicroseconds(uint16_t us) override {
    ++delayMicrosecondsCalls;
    lastDelayUs = us;
  }

  uint8_t inputPullupCalls;
  uint8_t driveLowCalls;
  uint8_t releaseCalls;
  uint8_t readCalls;
  uint8_t delayMicrosecondsCalls;
  uint16_t lastDelayUs;
  bool readValue;
  bool pinStates[32];
};

}  // namespace

int main() {
  FakeMatrixIO io;
  via::Pin rows[] = {};
  via::Pin cols[] = {};
  uint32_t raw[1], candidate[1], stable[1], changed[1];
  via::MatrixConfig cfg = {0, 0, rows, cols, via::kColToRow, 30, 5,
                            raw, candidate, stable, changed};
  via::Matrix matrix(cfg, io);
  assert(!matrix.begin());
  return 0;
}
