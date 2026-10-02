#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

// The S3 sketch keeps its `via::esp32s3` namespace, but NVS storage is shared
// with the generic ESP32 adapter.
#include "VIA_ESP32_NVS.h"

namespace via {
namespace esp32s3 {

using NVSStorage = esp32::NVSStorage;

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3