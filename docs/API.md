# API Reference

## Matrix Scanner

`#include <VIA_Matrix.h>`

```cpp
via::Pin rowPins[5]  = {5, 6, 7, 15, 16};
via::Pin colPins[15] = {8, 9, 10, 11, 12, 13, 14, 17, 18, 19, 20, 21, 39, 40, 41};
uint32_t rawRows[5], candidateRows[5], stableRows[5], changedRows[5];

via::MatrixIOArduino matrixIO; // platform adapter
via::MatrixConfig cfg = {5, 15, rowPins, colPins, via::kColToRow, 30, 5,
                          rawRows, candidateRows, stableRows, changedRows};
via::Matrix matrix(cfg, matrixIO);

matrix.begin();                  // returns false on zero dimensions
matrix.task(millis());           // scan + debounce, call per loop iteration
matrix.hasChanged();             // true -> changedRows populated, then resets
matrix.stableRow(2);             // debounced row 2 as uint32_t bitmask
matrix.rows();                   // configured row count
matrix.columns();                // configured column count
```

All row buffers are caller-owned `uint32_t[N]`. Maximum 32 columns. Diode
direction is `kColToRow` or `kRowToCol`, matching QMK `COL2ROW`/`ROW2COL`.
Default settle 30 µs, default debounce 5 ms. Zero debounce publishes
immediately.

## Keyboard Engine

`#include <VIA_Keyboard.h>`

```cpp
uint16_t activeCodes[5 * 15] = {}; // caller-owned, one per matrix position
via::KeyboardCallbacks cb;          // hostLedsChanged, bootloaderRequested
via::Keyboard kb({5, 15}, matrix, protocol, hid, activeCodes, &cb);

kb.begin();
kb.task(millis());                  // press/release, layers, 6KRO report
kb.stableRow(0);                    // stable debounced row for VIA tester
```

Supported keycodes: basic HID usages (0x0004-0x00A4), physical modifiers
(0x00E0-0x00E7), QK_MODS (0x0100-0x1FFF), MO/TG/TO/DF, QK_BOOT. All other
values are safe no-ops.

## Keycode Classification

`#include <VIA_Keycodes.h>`

Header-only QMK 0.0.8 range decoder:

```cpp
via::KeycodeType t = via::classifyKeycode(0x0004); // kBasic
uint8_t usage = via::extractBasicUsage(0x0004);    // 0x04
uint8_t mod   = via::extractModifierMask(0x00E0);  // 0x01

uint8_t hidUsage, modMask;
via::extractQkMods(0x1204, hidUsage, modMask);     // true, usage 0x04, mod 0x20

uint8_t action, layer;
via::extractLayerAction(0x5220, action, layer);    // true, action 0 (MO), layer 0
```

## Encoder

`#include <VIA_Encoder.h>`

```cpp
via::Encoder enc;
enc.setDebounceUs(2000);           // default 2000 µs, 0 disables
enc.update(digitalRead(A), digitalRead(B), millis());
int32_t delta = enc.consume();     // returns accumulated count, then resets to 0
int32_t raw = enc.count();        // current count without reset
```

Each full detent produces ±1. Negative is counterclockwise, positive is
clockwise. The state machine uses 4-step quadrature. Inject the delta into
`Protocol::encoderKeycode()` during `Keyboard::task()`.

## Battery

`#include <VIA_Battery.h>`

```cpp
via::BatteryMgr bat;
bat.setVref(5000);                 // ADC reference in mV (default 3300)
bat.setCalibration(3200, 4200);    // min/max voltage in mV
bat.setAverageSamples(32);         // rolling average window (default 32)
bat.charging(true);                // set charging flag
bat.update(analogRead(ADC_PIN), millis());
uint8_t pct = bat.percentage();    // 0-100, clamped
uint16_t adcVal = bat.rawFromMv(3700); // mV to ADC count
```

## Sleep Manager

`#include <VIA_SleepMgr.h>`

```cpp
via::SleepMgr sleep;
sleep.configure(300000);           // 5-minute idle timeout in ms
bool anyActivity = matrix.hasChanged() || encoder.consume() != 0;
bool shouldSleep = sleep.update(anyActivity, millis());

if (shouldSleep) {
  protocol.save();
  esp_deep_sleep_start();
}
```

Activity resets the timer. The manager returns `true` exactly once when the
timeout expires. Idle detection and wake are platform responsibilities.

## LayerState

`#include <VIA_Keyboard.h>`

```cpp
via::LayerState layers;
layers.begin(4);                   // 4 dynamic layers
layers.applyLayerPress(0, via::extractLayerAction(...)); // MO/TG/TO/DF
layers.applyLayerRelease(0, ...);  // MO only
uint16_t code = layers.resolve(defaultLayer, keymapPtr, row, col, rows, cols);
```

`MO(n)` uses reference counting. `TG(n)` toggles. `TO(n)` clears transients
and activates `n`. `DF(n)` changes the default layer.

## Protocol

`#include <VIA_Protocol.h>`

The existing VIA protocol v13 core is unchanged. See `docs/PORTING.md` and
`README.md` for `Config`, storage, Transport, callbacks, and supported
commands.

## Adapters

### RP2040 (TinyUSB)

`#include <VIA_TinyUSB_RawHID.h>` — VIA Raw HID (0xFF60/0x61).  
`#include <VIA_TinyUSB_Keyboard.h>` — boot keyboard HID.  
`#include <VIA_EEPROMStorage.h>` — EEPROM persistence.

### STM32F103

`#include <VIA_STM32F1_GPIO.h>` — Arduino `pinMode`/`digitalRead`.  
`#include <VIA_STM32F1_USB.h>` — Cube USB composite (keyboard + VIA).  
`#include <VIA_STM32F1_Flash.h>` — dual-slot atomic flash.  
`#include <VIA_STM32F1_Boot.h>` — ROM USART bootloader coordinator.

### ESP32-S3

`#include <VIA_ESP32S3_GPIO.h>` — Arduino `pinMode`/`digitalRead`.  
`#include <VIA_ESP32S3_NVS.h>` — Preferences NVS persistence.  
`#include <VIA_ESP32S3_BLE.h>` — NimBLE BLE HID keyboard.  
`#include <VIA_ESP32S3_BLE_ViaTransport.h>` — NimBLE GATT VIA transport (AirVIA).

### nRF52840 / Bluefruit Support

- `BLEKeyboardHID`: Implements HID keyboard reports over Bluefruit BLE.
- `BLEViaTransport`: Implements VIA protocol over BLE GATT (AirVIA). Note: Bluefruit must not be initialized globally before the adapter. It enforces single-stack ownership.
- `InternalFSStorage`: Uses LittleFS on internal flash for persistence. Requires a dual-slot record strategy. No auto-format is performed to protect existing user data.
- `MatrixIOArduino`: Uses standard Arduino GPIO functions.
