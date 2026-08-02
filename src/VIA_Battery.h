#pragma once

#include <stdint.h>

namespace via {

class BatteryMgr {
 public:
  void setCalibration(uint16_t minMv, uint16_t maxMv) {
    minMv_ = minMv;
    maxMv_ = maxMv;
  }

  void setAverageSamples(uint8_t n) { avgSamples_ = n; }

  void update(uint16_t rawAdc, uint32_t /*now*/) {
    uint16_t mv = rawToMv(rawAdc);
    if (mv < minMv_) mv = minMv_;
    if (mv > maxMv_) mv = maxMv_;

    sampleSum_ += mv;
    sampleSum_ -= circular_[sampleIndex_];
    circular_[sampleIndex_] = mv;
    if (sampleCount_ < avgSamples_) sampleCount_++;

    sampleIndex_++;
    if (sampleIndex_ >= avgSamples_) sampleIndex_ = 0;
  }

  uint8_t percentage() const {
    if (sampleCount_ == 0) return 0;
    uint16_t avgMv = sampleSum_ / sampleCount_;
    if (avgMv <= minMv_) return 0;
    if (avgMv >= maxMv_) return 100;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(avgMv - minMv_) * 100) /
        (maxMv_ - minMv_));
  }

  bool charging() const { return charging_; }
  void charging(bool state) { charging_ = state; }

  static uint16_t rawToMv(uint16_t raw, uint16_t vrefMv = 3300,
                           uint16_t adcBits = 12) {
    uint32_t maxRaw = (1UL << adcBits) - 1;
    return static_cast<uint16_t>((static_cast<uint32_t>(raw) * vrefMv) / maxRaw);
  }

  uint16_t rawFromMv(uint16_t mv, uint16_t vrefMv = 3300,
                      uint16_t adcBits = 12) const {
    uint32_t maxRaw = (1UL << adcBits) - 1;
    return static_cast<uint16_t>((static_cast<uint32_t>(mv) * maxRaw) / vrefMv);
  }

 private:
  uint16_t minMv_ = 3200;
  uint16_t maxMv_ = 4200;
  uint8_t avgSamples_ = 32;
  bool charging_ = false;

  // ponytail: circular buffer as flat array, no heap
  uint16_t circular_[32] = {};
  uint8_t sampleIndex_ = 0;
  uint8_t sampleCount_ = 0;
  uint32_t sampleSum_ = 0;
};

}  // namespace via
