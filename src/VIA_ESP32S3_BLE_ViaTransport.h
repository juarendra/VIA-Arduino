#pragma once

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)

// The S3 dual-mode sketch keeps its `via::esp32s3` namespace, but the AirVIA
// GATT transport implementation is shared with the generic ESP32 adapter.
#include "VIA_ESP32_BLE_ViaTransport.h"

namespace via {
namespace esp32s3 {

using BLEViaTransport = esp32::BLEViaTransport;

}  // namespace esp32s3
}  // namespace via

#endif  // ARDUINO_ARCH_ESP32 && CONFIG_IDF_TARGET_ESP32S3