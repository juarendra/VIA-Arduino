#pragma once

#include <stdint.h>

namespace via {

class SleepMgr {
 public:
  void configure(uint32_t timeoutMs) { timeoutMs_ = timeoutMs; }

  bool update(bool anyActivity, uint32_t now) {
    if (anyActivity) {
      lastActivityTs_ = now;
      sleepRequested_ = false;
      return false;
    }
    if (sleepRequested_) return false;
    uint32_t elapsed = now - lastActivityTs_;
    if (elapsed >= timeoutMs_) {
      sleepRequested_ = true;
      return true;
    }
    return false;
  }

  bool sleepRequested() const { return sleepRequested_; }

 private:
  uint32_t timeoutMs_ = 300000;
  uint32_t lastActivityTs_ = 0;
  bool sleepRequested_ = false;
};

}  // namespace via
