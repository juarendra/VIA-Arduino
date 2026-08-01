# Porting VIA_Arduino to a native-USB board

## 1. Provide the Raw HID interface

Create a *separate* vendor-defined HID interface in the USB composite device.
It must use the following report descriptor shape:

```text
Usage Page  0xFF60
Usage       0x61
Collection  Application
  Input     32 bytes
  Output    32 bytes
End Collection
```

Do not add a report ID to this interface. VIA selects the interface by usage
page and usage, then exchanges fixed 32-byte packets. The keyboard, mouse, and
consumer reports may be separate interfaces or collections according to the
USB stack, but Raw HID must remain discoverable as its own vendor interface.

## 2. Implement `via::Transport`

```cpp
class MyRawHidTransport : public via::Transport {
 public:
  bool receive(uint8_t packet[via::kPacketSize]) override {
    // Return true only after reading exactly 32 bytes from the OUT endpoint.
  }
  bool send(const uint8_t packet[via::kPacketSize]) override {
    // Send exactly 32 bytes to the IN endpoint.
  }
};
```

Call `keyboard.task(millis())` often from `loop()`. It receives one request,
updates the supplied buffer as the VIA response, and sends it back.

If the application owns packet I/O instead, call
`keyboard.process(packet, millis())` on each mutable 32-byte request and send
the resulting packet yourself. Direct processing cannot confirm delivery and
therefore never invokes `bootloaderJump()`; use `task()` when that operation is
enabled.

### RP2040 reference adapter

`VIA_TinyUSB_RawHID.h` provides the first concrete adapter for RP2040 boards
using the Earle Philhower Arduino-Pico core with **Tools → USB Stack → Adafruit
TinyUSB**. Install **Adafruit TinyUSB Library**, create `via::tinyusb::RawHID`,
call `begin()` before USB enumeration, and pass it to `via::Protocol`.

The reference adapter deliberately provides only the vendor Raw HID interface.
Create a second TinyUSB HID interface for keyboard reports in the application;
the two interfaces must not share report IDs. See
[`RP2040_VIA_RawHID`](../examples/RP2040_VIA_RawHID/RP2040_VIA_RawHID.ino).

## 3. Implement `via::Storage`

Use a reserved region that is large enough for:

```text
payload = (rows × columns × layers × 2)
        + (layers × encoderCount × 2 directions × 2)
        + macroBytes + 4 layout bytes + customValueState
storage bytes = 12-byte header + payload
```

The built-in record has magic, version, payload size, and CRC32. Version 2 is
used by 0.2.0; version 1 records from 0.1.0 are rejected. On a rejected load,
`begin()` performs its normal one-time startup fallback to configured built-in
defaults. No automatic migration or save occurs, so users must reconfigure and
save a version 2 record. A production flash adapter should use two alternating
pages/slots so that a reset or power loss never destroys the last complete
record. Do not point a flash adapter at the bootloader, option bytes, or
application code.

Storage-backed protocols also require a separate static load workspace. It
must hold the complete payload. The core rejects overlap with configured
keymap, encoder-map, and macro buffers:

```cpp
static uint8_t loadBuffer[sizeof(keymap) + sizeof(encoderMap) +
                          sizeof(macros) + sizeof(uint32_t) + customStateBytes];

via::Config config = {
    rows, columns, layers, keymap, defaultKeymap,
    macros, sizeof(macros), macroCount, firmwareVersion, autoSaveMs,
    defaultLayoutOptions, encoderCount, encoderMap, defaultEncoderMap,
    loadBuffer, sizeof(loadBuffer),
};
via::Protocol keyboard(config, transport, &storage, customValue);
```

`keyboard.requiredLoadBufferSize()` returns the exact payload size, or `0` when
the payload cannot fit the supported record. `begin()` returns false before
storage access when a storage-backed configuration omits the workspace,
provides too few bytes, or aliases configured keymap, encoder-map, or macro
buffers. The core cannot inspect active state owned by a `CustomValue`
implementation; the caller must keep that state outside this workspace. The
implementation must also leave active state unchanged when `loadState()`
returns false. Configurations without `Storage` do not need a load workspace.

`MemoryStorage` is only a test backend; it intentionally does not survive a
power cycle.

## 4. Configure protocol state and callbacks

The fields after `autoSaveMs` are optional and were appended in 0.2.0:

```text
defaultLayoutOptions
encoderCount, encoderMap, defaultEncoderMap
loadBuffer, loadBufferBytes
matrixStateEnabled, eepromResetEnabled, bootloaderEnabled
```

An encoder map contains `layers * encoderCount * 2` 16-bit entries. Direction
`0` is counterclockwise and direction `1` is clockwise. Layout options and
encoder maps are included in persisted state.

Override `Callbacks::matrixRow(uint8_t) -> uint32_t` to expose matrix-test
state, `deviceIndication(uint8_t)` for the complete indication value,
`layoutOptionsChanged(uint32_t)` for layout changes, `changed()` for mutable
state changes, and `bootloaderJump()` for a platform reset action.

Matrix disclosure, EEPROM reset, and bootloader jump default to disabled. Set
only the corresponding `Config` boolean after reviewing the application's
security requirements. A bootloader callback runs only after `task()` sends
the response successfully.

## 5. Connect physical features

The protocol knows the assigned QMK/VIA 16-bit keycode. Your sketch owns switch
scanning and maps each event to:

```cpp
uint16_t code = keyboard.keycode(activeLayer, row, column);
```

Interpret the keycode with your keyboard HID, mouse, consumer, and system
control implementation. This separation lets a small macro pad and a full
keyboard share the same VIA configuration core.

Matrix scanning, active-layer policy, and QMK keycode interpretation are not
implemented by the protocol core.

## 6. Test in VIA

1. Use a unique USB VID/PID appropriate for your product.
2. Create a V3 definition JSON that has the same matrix rows, columns, layers,
   and custom values as the firmware.
3. Load that JSON in VIA Design tab, leaving V2 definitions disabled.
4. Verify protocol version, key read/write, macros, custom values, save,
   unplug/replug, and factory reset.
