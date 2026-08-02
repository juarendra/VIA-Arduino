# ESP32-S3 Wireless VIA Keyboard Design

## Status

Approved design for planning. Phase `0.4`. No code written yet.

## Goal

Add ESP32-S3 support to VIA-Arduino library for a wireless Bluetooth keyboard
configurable via VIA protocol over USB while typing wirelessly via BLE HID.

## Reference Platform

- SoC: ESP32-S3-WROOM-1 (custom PCB, not dev kit).
- USB: USB-OTG via USB-C connector, TinyUSB stack.
- BLE: BLE 5.0 via NimBLE-Arduino, ESP32-BLE-Keyboard HID layer.
- Arduino core: `espressif/arduino-esp32` v3.0.x.
- Matrix: 5 rows × 15 columns, COL2ROW, active-low.
- Encoder: 1× EC11 rotary with push button.
- Display: SSD1306 OLED 128×32 I2C.
- RGB: WS2812 strip, power-gated via MOSFET.
- Battery: LiPo 3.7 V → TP4056 charger → 3.3 V LDO → ADC voltage sense.
- Persistence: NVS via ESP32 Preferences API.

## Scope

### Included

- Dual-mode HID: USB HID keyboard when plugged in, BLE HID keyboard when
  wireless.
- VIA protocol v13 over dedicated USB Raw HID interface (usage page 0xFF60,
  usage 0x61).
- VIA dynamic keymap, layout options, encoder mapping, and factory reset.
- NVS-backed persistence (Preferences read/write/commit).
- 5 ms deferred global debounce.
- Basic keyboard usages, physical modifiers, `QK_MODS`, `MO`, `TG`, `TO`,
  `DF`.
- Boot-protocol 6KRO reports.
- One rotary encoder with per-layer directional keycode mapping.
- WS2812 underglow with layer-indication effect.
- SSD1306 OLED showing current layer, battery percentage, and BLE connection
  state.
- Battery voltage monitoring via ADC, battery service notify via BLE.
- Idle deep sleep with GPIO wake from any matrix row or column pin.
- ESP32-S3 compile gate in CI.

### Deferred To Phase 0.5 Or Later

- Mod-Tap, Layer-Tap, combos, one-shot keys, and advanced QMK keycode
  execution.
- Consumer, system, and mouse HID reports.
- NKRO.
- Macro execution and encoder execution.
- Split keyboard BLE transport.
- Advanced RGB effects catalogue.
- Advanced power management (light sleep, dynamic scan rate, BLE connection
  interval tuning).
- Multiple BLE profiles or bond slots.
- RP2040 BLE and other MCU BLE integrations.

## Architecture

```text
Arduino sketch (ESP32S3_VIA_BLE.ino)
  └── loop()
        ├── usbTask()       → TinyUSB (Raw HID + keyboard)
        ├── protocolTask()  → VIA Protocol v13
        ├── keyboardTask()  → events, layers, 6KRO
        ├── bleTask()       → BLE HID typing
        ├── encoderTask()   → rotary gray-code
        ├── oledTask()      → display refresh
        ├── rgbTask()       → WS2812 effects
        ├── batteryTask()   → ADC read + BLE notify
        └── sleepMgr()      → idle timeout → deep sleep

Portable core (REUSE, unchanged):
  VIA_Matrix       scan + 5ms sym_defer_g debounce
  VIA_Keyboard     stable event batch, layer resolve, 6KRO
  VIA_Protocol     VIA protocol v13, dynamic keymap, persistence
  VIA_Keycodes     QMK 0.0.8 classification (header-only)
  VIA_RGBLight     existing custom-value channel 2

New portable modules:
  VIA_Encoder      gray-code state classifier, direction callback
  VIA_OLED         framebuffer, layer/battery/BLE rendering
  VIA_WS2812       color math, effect stepping, RMT output hook
  VIA_Battery      ADC scaling, percentage duty cycle
  VIA_SleepMgr     idle timeout state machine, wake reason dispatch

ESP32-S3 adapters (new):
  TinyUSB_RawHID   USB VIA transport (port from RP2040 adapter)
  TinyUSB_Keyboard USB HID keyboard (wired fallback)
  BLE_KeyboardHID  NimBLE BLE HID keyboard (wireless typing)
  GPIO_MatrixIO    Arduino pinMode/digitalRead/digitalWrite
  NVS_Storage      ESP32 Preferences as via::Storage
  ADC_Battery      analogRead voltage divider
  I2C_OLED         Wire + SSD1306
  RMT_WS2812       ESP32 RMT peripheral
```

The portable core files receive no changes in this phase. Adapters expose only
settable contracts and callback registrations; the sketch wires them together.

## Dual-Mode

USB discovery follows the WebHID contract at usage page `0xFF60`, usage
`0x61`, 32-byte IN/OUT, no report ID.

### USB Plugged In

- VBUS present: TinyUSB device starts, BLE advertising stops.
- Keyboard reports sent over USB HID interface.
- VIA protocol active on dedicated Raw HID interface.
- OLED shows charging indicator.

### Battery Only

- VBUS absent: TinyUSB stops, BLE advertising begins.
- Keyboard reports sent via ESP32-BLE-Keyboard (NimBLE backend).
- VIA protocol unreachable. No Raw HID interface exposed.
- OLED shows battery percentage.

### Transition

- Mode swap occurs at most once per `loop()` iteration.
- Any in-flight USB report completes before BLE takes over.
- All-release report sent on both the deactivated and newly activated
  transport.
- No held key state is lost or duplicated during the transition.

## BLE HID

Stack: NimBLE-Arduino v2.x as BLE host, ESP32-BLE-Keyboard v0.3.x as HID
device layer.

ESP32-BLE-Keyboard maps `via::KeyboardReport` struct fields directly to its
`sendReport` primitives. The 8-byte keyboard report (1 modifier + 1 reserved
+ 6 keys) is the standard BLE HOGP keyboard input report format.

BLE connection interval uses the NimBLE default. Paired host receives all
keyboard output. No multiplexing or multiple-bond support in this phase.

## VIA Protocol

The protocol core, custom-value channel, keymap, encoder map, layout options,
factory reset, and save/load semantics defined by `VIA_Protocol` are
unchanged.

The `TinyUSB_RawHID` adapter is a minimal port of the existing RP2040
`VIA_TinyUSB_RawHID`. Descriptor bytes, endpoint handling, `receive`/`send`/
`sendComplete` semantics, and static lifetime rules are identical. The only
change is the platform TinyUSB include path and USB peripheral configuration.

## NVS Persistence

`NVSStorage` implements `via::Storage` over ESP32 Preferences.

- `capacity()` returns 4096 bytes (allocated NVS partition).
- `read(offset, buf, len)` calls `Preferences::getBytes`.
- `write(offset, buf, len)` calls `Preferences::putBytes`.
- `commit()` calls `Preferences::end()` followed by `begin("via", false)`.
- `erase()` calls `Preferences::clear()` then re-opens the namespace.

NVS writes only occur on explicit VIA save or autosave. No wear-leveling
beyond what ESP32 NVS provides internally.

## Battery

ADC channel reads the battery voltage through a resistor divider. The task
runs at 1 Hz when the battery is discharging and at 10 Hz while charging.

A BLE battery service (0x180F) with Battery Level characteristic (0x2A19)
reports the integer percentage. The value is computed from a configurable
voltage-to-percentage curve (default: 3.2V = 0%, 4.2V = 100%, linear).

Reading is averaged over 32 samples to reject ADC noise.

## Encoder

A gray-code state machine tracks the last two bits of the A and B encoder
signals. On each stable quadrature transition the direction and count are
published.

`Keyboard::task()` polls encoder count, resolves the per-layer directional
keycode from `Protocol::encoderKeycode()`, and injects it as a synthetic
press-then-release pair into the normal event pipeline. No dedicated encoder
event queue is needed.

The encoder rotary directions (CW/CCW) and press button are mapped through
normal VIA encoder commands `0x14` and `0x15`.

## OLED

A 1 Hz refresh task reads the current layer from `LayerState`, battery from
`BatteryMgr`, and BLE state from `BLE_KeyboardHID`. It renders a 128x32
framebuffer and writes it over I2C to the SSD1306.

The layout shows layer index/name on the left, BLE icon in the center, and
battery percentage on the right. The display is turned off during deep sleep.

## RGB

One WS2812 data line driven by the ESP32 RMT peripheral. LED count is
configurable; the reference hardware uses 6 LEDs.

Effect: solid color mapped to the active layer index. Layer change triggers
a 500 ms transition. Brightness scales to a compile-time maximum.

The MOSFET gate between VCC and the LED strip is driven by a dedicated GPIO.
It is set high only while the effect is running and set low before deep
sleep.

## Sleep

An idle counter resets on any matrix change or encoder activity. When the
counter exceeds the configurable timeout (default 300 s) the state machine
saves dirty protocol state to NVS, sends an all-release BLE or USB report,
stops BLE advertising, turns off OLED, turns off RGB, configures GPIO wake
sources, and calls `esp_deep_sleep_start(5e6)`.

Wake sources are all matrix row and column pins configured as RTC GPIO
inputs with pull-up. The first press wakes the ESP32-S3. The first matrix
scan after wake is discarded to reject the wakeup bounce.

Deep sleep current target: less than 20 µA at 3.7 V.

## Memory Budget

| Item | Expected bytes |
|---|--:|
| Dynamic keymap (5*15*4*2) | 600 |
| Encoder map (4*1*2*2) | 16 |
| Protocol load buffer | 620 |
| 4 matrix row arrays | 40 |
| Active press-time codes (75) | 150 |
| Portable engine state | 200 |
| OLED framebuffer | 512 |
| NimBLE stack | 35000 |
| TinyUSB stack | 8000 |
| ESP32 Arduino + FreeRTOS | 50000 |
| **Total** | **~95,000** |
| **Available** | **512,000** |

Ample margin remains for future features and PSRAM is not required.

## Board Pin Contract

| Function | GPIO |
|---|---|
| Matrix rows (5) | 5, 6, 7, 15, 16 |
| Matrix columns (15) | 8-14, 17-21, 39-41 |
| Encoder A | 4 |
| Encoder B | 3 |
| Encoder button | 2 |
| OLED SDA | 1 |
| OLED SCL | 42 |
| WS2812 data | 38 |
| RGB MOSFET gate | 37 |
| VBUS sense | 48 |
| Battery ADC | 47 |
| USB D- | 19 |
| USB D+ | 20 |

JTAG USB (GPIO 18/19) is available for debugging via the built-in USB
Serial/JTAG controller.

## Testing

### Native Tests

- Encoder gray-code state machine: all 4 quadrature states, CW/CCW count,
  direction reversal, noise rejection, debounce.
- WS2812: color HSV-to-RGB math, effect step timing, brightness scaling,
  zero LEDs.
- Battery: ADC value mapping at bounds, mid-range, and floating-point
  interpolation.
- Sleep manager: idle timeout expiry, dirty save gate, wake reason dispatch,
  transitions prevent re-entry.

### CI

- Existing protocol, matrix, keyboard, descriptor, flash, and RP2040
  TinyUSB jobs remain green.
- ESP32-S3 compile job added: install `espressif/esp32` core, compile
  reference sketch with TinyUSB + NimBLE as dependencies.
- Library lint remains green.

### Hardware Certification

Custom PCB hardware gates:

- USB enumeration shows two HID interfaces (keyboard + VIA Raw HID).
- VIA desktop discovers the keyboard, reads protocol version, and performs
  keymap remap.
- Remapped VIA keymap persists after NVS save and cold boot.
- BLE HID typing works on Windows, macOS, Linux, and Android.
- No key drops or repeats at BLE connection interval 15 ms.
- USB-to-BLE mode switch produces no stuck keys.
- Idle timeout triggers deep sleep. Wake from any key restores BLE
  connection.
- Battery percentage is reported correctly via BLE battery service.
- Sleep current is under 20 µA.
- Encoder CW/CCW directions and layer mapping are correct.
- OLED shows current layer, battery, and BLE state.
- WS2812 color changes with active layer.
- Factory reset clears NVS and restores compiled defaults.
- Programming via USB works without BOOT button (auto-reset from USB
  Serial/JTAG).

## References

- [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [arduino-esp32 v3.0](https://github.com/espressif/arduino-esp32/releases)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)
- [Adafruit TinyUSB](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)
- [QMK layers](https://docs.qmk.fm/feature_layers)
- [VIA specification](https://www.caniusevia.com/docs/specification)
