#pragma once

#include <stdint.h>

namespace via {

typedef uint32_t Pin;

enum DiodeDirection { kColToRow, kRowToCol };

class MatrixIO {
 public:
  virtual ~MatrixIO() {}
  virtual void inputPullup(Pin) {}
  virtual void driveLow(Pin) {}
  virtual void release(Pin) {}
  virtual bool read(Pin) { return false; }
  virtual void delayMicroseconds(uint16_t) {}
};

struct MatrixConfig {
  uint8_t rows;
  uint8_t columns;
  const Pin* rowPins;
  const Pin* columnPins;
  DiodeDirection direction;
  uint16_t settleUs;
  uint16_t debounceMs;
  uint32_t* rawRows;
  uint32_t* candidateRows;
  uint32_t* stableRows;
  uint32_t* changedRows;
};

class Matrix {
 public:
  Matrix(const MatrixConfig& config, MatrixIO& io) : config_(config), io_(io) {}

  bool begin() { return !(config_.rows == 0 || config_.columns == 0); }

  void task(uint32_t ms);

  uint32_t rawRow(uint8_t r) const { return config_.rawRows[r]; }
  uint32_t stableRows() const { return 0; }
  uint32_t changedRows() const { return 0; }

  uint8_t rows() const { return config_.rows; }
  uint8_t columns() const { return config_.columns; }

 private:
  const MatrixConfig& config_;
  MatrixIO& io_;
};

}  // namespace via
