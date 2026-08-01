#pragma once

#if defined(ARDUINO_ARCH_STM32) || defined(STM32F1xx)

#include "VIA_Keyboard.h"
#include "VIA_Protocol.h"

#include <stdint.h>

namespace via {
namespace stm32f1 {

class BootCoordinator {
 public:
  BootCoordinator(via::Protocol& protocol, via::KeyboardHID& keyboardHid,
                   via::KeyboardCallbacks& callbacks);

  bool request();
  void task();
  bool active() const { return state_ != kIdle; }

 private:
  enum State {
    kIdle,
    kDirtySave,
    kKeyboardRelease,
    kViaRelease,
    kDetach,
    kJump
  };

  static void loadSystemMemoryVector();

  via::Protocol& protocol_;
  via::KeyboardHID& keyboardHid_;
  via::KeyboardCallbacks& callbacks_;
  State state_;
};

}  // namespace stm32f1
}  // namespace via

#endif  // ARDUINO_ARCH_STM32 || STM32F1xx
