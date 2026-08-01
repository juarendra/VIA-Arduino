#include "VIA_Matrix.h"

namespace via {

void Matrix::task(uint32_t) {
  if (config_.direction != kColToRow) return;

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
}

}  // namespace via
