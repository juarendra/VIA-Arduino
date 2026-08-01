#include "VIA_Matrix.h"

namespace via {

void Matrix::task(uint32_t) {
  if (config_.direction == kColToRow) {
    for (uint8_t r = 0; r < config_.rows; ++r) {
      io_.driveLow(config_.rowPins[r]);
      io_.delayMicroseconds(config_.settleUs);

      uint32_t rowVal = 0;
      for (uint8_t c = 0; c < config_.columns; ++c) {
        if (!io_.read(config_.columnPins[c]))
          rowVal |= (1UL << c);
      }
      config_.rawRows[r] = rowVal;

      io_.release(config_.rowPins[r]);
    }
  } else {
    for (uint8_t r = 0; r < config_.rows; ++r) {
      config_.rawRows[r] = 0;
      io_.inputPullup(config_.rowPins[r]);
    }
    for (uint8_t c = 0; c < config_.columns; ++c)
      io_.release(config_.columnPins[c]);

    for (uint8_t c = 0; c < config_.columns; ++c) {
      io_.driveLow(config_.columnPins[c]);
      io_.delayMicroseconds(config_.settleUs);

      for (uint8_t r = 0; r < config_.rows; ++r) {
        if (!io_.read(config_.rowPins[r]))
          config_.rawRows[r] |= (1UL << c);
      }

      io_.release(config_.columnPins[c]);
    }
  }
}

}  // namespace via
