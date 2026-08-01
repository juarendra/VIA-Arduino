#include <assert.h>
#include "VIA_Matrix.h"

namespace {

class FakeMatrixIO : public via::MatrixIO {
 public:
  FakeMatrixIO() : inputPullupCalls(0), driveLowCalls(0), releaseCalls(0),
                    readCalls(0), delayMicrosecondsCalls(0), readValue(false) {
    for (uint8_t i = 0; i < sizeof(pinStates); ++i) pinStates[i] = true;
  }

  void inputPullup(via::Pin pin) override {
    ++inputPullupCalls;
    pinStates[pin & 0x1F] = true;
  }
  void driveLow(via::Pin pin) override {
    ++driveLowCalls;
    pinStates[pin & 0x1F] = false;
  }
  void release(via::Pin pin) override {
    ++releaseCalls;
    pinStates[pin & 0x1F] = true;
  }
  bool read(via::Pin pin) override {
    ++readCalls;
    return pinStates[pin & 0x1F];
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

  matrix.task(0);
  assert(matrix.stableRows() == 0);
  assert(matrix.changedRows() == 0);

  {
    via::Pin rowPins[2] = {100, 101};
    via::Pin colPins[3] = {200, 201, 202};
    uint32_t rawRows[2] = {0, 0};
    uint32_t candidate[2] = {0, 0};
    uint32_t stable[2] = {0, 0};
    uint32_t changed[2] = {0, 0};
    FakeMatrixIO scanIO;
    scanIO.pinStates[200 & 0x1F] = false;
    via::MatrixConfig scanCfg = {2, 3, rowPins, colPins, via::kColToRow, 30, 5,
                                 rawRows, candidate, stable, changed};
    via::Matrix scanMatrix(scanCfg, scanIO);
    assert(scanMatrix.begin());
    scanMatrix.task(0);
    assert(scanMatrix.rawRow(0) == 0x01);
  }

  return 0;
}
