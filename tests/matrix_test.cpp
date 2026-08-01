#include <assert.h>
#include "VIA_Matrix.h"

namespace {

class FakeMatrixIO : public via::MatrixIO {
 public:
  FakeMatrixIO() : inputPullupCalls(0), driveLowCalls(0), releaseCalls(0),
                    readCalls(0), delayMicrosecondsCalls(0), readValue(false),
                    readPos(0), drivenIdx(0) {
    for (uint8_t i = 0; i < sizeof(keyMap); ++i) keyMap[i] = 0;
  }

  void inputPullup(via::Pin) override { ++inputPullupCalls; }
  void driveLow(via::Pin pin) override {
    ++driveLowCalls;
    drivenIdx = pin & 0x1F;
    readPos = 0;
  }
  void release(via::Pin) override { ++releaseCalls; }
  bool read(via::Pin) override {
    ++readCalls;
    return !(keyMap[drivenIdx] & (1U << readPos++));
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
  uint8_t keyMap[32];
 private:
  uint8_t readPos;
  uint8_t drivenIdx;
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
    scanIO.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig scanCfg = {2, 3, rowPins, colPins, via::kColToRow, 30, 5,
                                 rawRows, candidate, stable, changed};
    via::Matrix scanMatrix(scanCfg, scanIO);
    assert(scanMatrix.begin());
    scanMatrix.task(0);
    assert(scanMatrix.rawRow(0) == 0x01);
  }

  // ROW2COL: drive columns low, read rows
  {
    via::Pin rowPins[2] = {100, 101};
    via::Pin colPins[3] = {200, 201, 202};
    uint32_t rawRows[2] = {0, 0};
    uint32_t candidate[2] = {0, 0};
    uint32_t stable[2] = {0, 0};
    uint32_t changed[2] = {0, 0};
    FakeMatrixIO io2;
    io2.keyMap[202 & 0x1F] = 0x01;
    via::MatrixConfig cfg2 = {2, 3, rowPins, colPins, via::kRowToCol, 30, 5,
                              rawRows, candidate, stable, changed};
    via::Matrix matrix2(cfg2, io2);
    assert(matrix2.begin());
    matrix2.task(0);
    assert(matrix2.rawRow(0) == (1U << 2));
  }

  // DEBOUNCE: zero debounce publishes immediately
  {
    via::Pin rowPins[1] = {100};
    via::Pin colPins[1] = {200};
    uint32_t rawRows[1] = {0};
    uint32_t candidate[1] = {0};
    uint32_t stable[1] = {0};
    uint32_t changed[1] = {0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig cfg = {1, 1, rowPins, colPins, via::kColToRow, 30, 0,
                             rawRows, candidate, stable, changed};
    via::Matrix matrix(cfg, io);
    assert(matrix.begin());
    matrix.task(0);
    assert(matrix.stableRows() == 0x01);
    assert(matrix.changedRows() == 0x01);
  }

  // DEBOUNCE: 10ms debounce delays publication
  {
    via::Pin rowPins[1] = {100};
    via::Pin colPins[1] = {200};
    uint32_t rawRows[1] = {0};
    uint32_t candidate[1] = {0};
    uint32_t stable[1] = {0};
    uint32_t changed[1] = {0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig cfg = {1, 1, rowPins, colPins, via::kColToRow, 30, 10,
                             rawRows, candidate, stable, changed};
    via::Matrix matrix(cfg, io);
    assert(matrix.begin());
    matrix.task(0);
    assert(matrix.stableRows() == 0);
    assert(matrix.changedRows() == 0);
    matrix.task(9);
    assert(matrix.stableRows() == 0);
    assert(matrix.changedRows() == 0);
    matrix.task(10);
    assert(matrix.stableRows() == 0x01);
    assert(matrix.changedRows() == 0x01);
  }

  // DEBOUNCE: noise spike resets timer, publication delayed
  {
    via::Pin rowPins[1] = {100};
    via::Pin colPins[1] = {200};
    uint32_t rawRows[1] = {0};
    uint32_t candidate[1] = {0};
    uint32_t stable[1] = {0};
    uint32_t changed[1] = {0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig cfg = {1, 1, rowPins, colPins, via::kColToRow, 30, 10,
                             rawRows, candidate, stable, changed};
    via::Matrix matrix(cfg, io);
    assert(matrix.begin());
    matrix.task(0);
    io.keyMap[100 & 0x1F] = 0x00;
    matrix.task(2);
    io.keyMap[100 & 0x1F] = 0x01;
    matrix.task(3);
    matrix.task(12);
    assert(matrix.stableRows() == 0);
    matrix.task(13);
    assert(matrix.stableRows() == 0x01);
    assert(matrix.changedRows() == 0x01);
  }

  // DEBOUNCE: millis() wrap handled with unsigned subtraction
  {
    via::Pin rowPins[1] = {100};
    via::Pin colPins[1] = {200};
    uint32_t rawRows[1] = {0};
    uint32_t candidate[1] = {0};
    uint32_t stable[1] = {0};
    uint32_t changed[1] = {0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig cfg = {1, 1, rowPins, colPins, via::kColToRow, 30, 10,
                             rawRows, candidate, stable, changed};
    via::Matrix matrix(cfg, io);
    assert(matrix.begin());
    uint32_t wrap = (uint32_t)~0UL;
    matrix.task(wrap - 2);
    matrix.task(wrap - 1);
    assert(matrix.stableRows() == 0);
    matrix.task(0);
    assert(matrix.stableRows() == 0);
    matrix.task(6);
    assert(matrix.stableRows() == 0);
    matrix.task(7);
    assert(matrix.stableRows() == 0x01);
    assert(matrix.changedRows() == 0x01);
  }

  // DEBOUNCE: 5ms default timing
  {
    via::Pin rowPins[1] = {100};
    via::Pin colPins[1] = {200};
    uint32_t rawRows[1] = {0};
    uint32_t candidate[1] = {0};
    uint32_t stable[1] = {0};
    uint32_t changed[1] = {0};
    FakeMatrixIO io;
    io.keyMap[100 & 0x1F] = 0x01;
    via::MatrixConfig cfg = {1, 1, rowPins, colPins, via::kColToRow, 30, 5,
                             rawRows, candidate, stable, changed};
    via::Matrix matrix(cfg, io);
    assert(matrix.begin());
    matrix.task(0);
    assert(matrix.stableRows() == 0);
    matrix.task(4);
    assert(matrix.stableRows() == 0);
    matrix.task(5);
    assert(matrix.stableRows() == 0x01);
    assert(matrix.changedRows() == 0x01);
  }

  return 0;
}
