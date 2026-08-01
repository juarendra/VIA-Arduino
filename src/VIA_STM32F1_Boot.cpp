#include "VIA_STM32F1_Boot.h"

#if defined(ARDUINO_ARCH_STM32) || defined(STM32F1xx)

namespace via {
namespace stm32f1 {

BootCoordinator::BootCoordinator(via::Protocol& protocol,
                                   via::KeyboardHID& keyboardHid,
                                   via::KeyboardCallbacks& callbacks)
    : protocol_(protocol),
      keyboardHid_(keyboardHid),
      callbacks_(callbacks),
      state_(kIdle) {}

bool BootCoordinator::request() {
  if (state_ != kIdle) return false;
  state_ = kDirtySave;
  return true;
}

void BootCoordinator::task() {
  switch (state_) {
    case kIdle:
      return;

    case kDirtySave: {
      if (protocol_.dirty() && !protocol_.save()) {
        state_ = kIdle;
        return;
      }
      via::KeyboardReport release = {};
      if (keyboardHid_.configured()) {
        keyboardHid_.send(release);
      }
      state_ = kKeyboardRelease;
      return;
    }

    case kKeyboardRelease:
      if (!keyboardHid_.configured() || keyboardHid_.sendComplete()) {
        state_ = kViaRelease;
      }
      return;

    case kViaRelease:
      state_ = kDetach;
      return;

    case kDetach:
      __disable_irq();
      loadSystemMemoryVector();
      return;

    case kJump:
      return;
  }
}

void BootCoordinator::loadSystemMemoryVector() {
  const uint32_t sysMemBase = 0x1FFFF000U;
  uint32_t msp = *reinterpret_cast<const uint32_t*>(sysMemBase);
  uint32_t resetVector = *reinterpret_cast<const uint32_t*>(sysMemBase + 4);

  __set_MSP(msp);

  void (*jump)(void) = reinterpret_cast<void (*)(void)>(resetVector);
  jump();

  while (true) {}
}

}  // namespace stm32f1
}  // namespace via

#endif  // ARDUINO_ARCH_STM32 || STM32F1xx
