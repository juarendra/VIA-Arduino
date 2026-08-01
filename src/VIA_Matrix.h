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
  Matrix(const MatrixConfig& config, MatrixIO& io) : config_(config), io_(io),
      lastRawChangeTS_(0), debouncePending_(false) {
    for (uint8_t r = 0; r < config_.rows; ++r) prevRaw_[r] = 0;
  }

  bool begin() { return !(config_.rows == 0 || config_.columns == 0); }

  void task(uint32_t ms);

  uint32_t rawRow(uint8_t r) const { return config_.rawRows[r]; }
  uint32_t stableRow(uint8_t r) const { return config_.stableRows[r]; }
  uint32_t changedRow(uint8_t r) const { return config_.changedRows[r]; }
  uint32_t stableRows() const {
    uint32_t mask = 0;
    for (uint8_t r = 0; r < config_.rows; ++r)
      if (config_.stableRows[r]) mask |= (1UL << r);
    return mask;
  }
  uint32_t changedRows() const {
    uint32_t mask = 0;
    for (uint8_t r = 0; r < config_.rows; ++r)
      if (config_.changedRows[r]) mask |= (1UL << r);
    return mask;
  }

  bool hasChanged() {
    for (uint8_t r = 0; r < config_.rows; ++r)
      if (config_.changedRows[r]) {
        for (uint8_t j = 0; j < config_.rows; ++j)
          config_.changedRows[j] = 0;
        return true;
      }
    return false;
  }

  uint32_t nextScan(uint32_t now) const {
    if (!debouncePending_) return 0;
    uint32_t elapsed = now - lastRawChangeTS_;
    uint32_t remaining = config_.debounceMs;
    if (elapsed < remaining) return remaining - elapsed;
    return 0;
  }

  uint8_t rows() const { return config_.rows; }
  uint8_t columns() const { return config_.columns; }

 private:
  const MatrixConfig& config_;
  MatrixIO& io_;
  uint32_t prevRaw_[8];
  uint32_t lastRawChangeTS_;
  bool debouncePending_;
};

}  // namespace via
