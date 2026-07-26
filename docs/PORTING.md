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

## 3. Implement `via::Storage`

Use a reserved region that is large enough for:

```text
sizeof(header) + (rows × columns × layers × 2) + macroBytes + customValueState
```

The built-in record has magic, version, payload size, and CRC32. A production
flash adapter should use two alternating pages/slots so that a reset or power
loss never destroys the last complete record. Do not point a flash adapter at
the bootloader, option bytes, or application code.

`MemoryStorage` is only a test backend; it intentionally does not survive a
power cycle.

## 4. Connect physical features

The protocol knows the assigned QMK/VIA 16-bit keycode. Your sketch owns switch
scanning and maps each event to:

```cpp
uint16_t code = keyboard.keycode(activeLayer, row, column);
```

Interpret the keycode with your keyboard HID, mouse, consumer, and system
control implementation. This separation lets a small macro pad and a full
keyboard share the same VIA configuration core.

## 5. Test in VIA

1. Use a unique USB VID/PID appropriate for your product.
2. Create a V3 definition JSON that has the same matrix rows, columns, layers,
   and custom values as the firmware.
3. Load that JSON in VIA Design tab, leaving V2 definitions disabled.
4. Verify protocol version, key read/write, macros, custom values, save,
   unplug/replug, and factory reset.
