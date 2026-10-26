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
    // Return true once exactly 32 bytes are accepted by the IN endpoint.
  }
  bool sendComplete() override {
    // Return false while that accepted transfer remains in flight.
  }
};
```

`Protocol` retries the same response only while `send()` returns false. It calls
`sendComplete()` only after a true return. Synchronous transports can omit the
override and inherit immediate completion; asynchronous transports must report
endpoint completion. A disconnected asynchronous endpoint therefore leaves a
boot response pending rather than jumping or relying on a timeout.

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

`RawHID` keeps one static HID interface, callback, and receive buffer alive for
the device lifetime. In Adafruit TinyUSB 3.7.7, a failed `addInterface()` returns
from `Adafruit_USBD_HID::begin()` before HID instance registration. A successful
begin does register the HID object, and TinyUSB provides no unregister path;
this successful lifetime is why the object and callback storage are static.

The adapter deliberately permits exactly one `begin()` attempt per device reset,
even when upstream registration fails. A failed attempt leaves no registered
callback target, clears wrapper ownership, and conservatively rejects retries or
replacement wrappers until reset. Destroying a successful owner also clears only
wrapper ownership; the registered static interface remains alive and later
reports are safely dropped. Keep a successful wrapper alive for as long as the
application needs VIA transport.

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

Storage-backed protocols also require a separate static load/reset workspace.
It must hold the complete payload. The core rejects overlap with mutable or
default keymap and encoder-map buffers, and with the macro buffer:

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
provides too few bytes, or aliases configured mutable/default keymap,
mutable/default encoder-map, or macro buffers. The core cannot inspect active
state owned by a `CustomValue`
implementation; the caller must keep that state outside this workspace. The
implementation must also leave active state unchanged when `loadState()`
returns false. Configurations without `Storage` do not need a load workspace.

`load()` performs one payload read into this workspace. CRC and custom-state
validation cover those exact staged bytes before any active state changes. On
success, state and layout callbacks publish before dirty state clears; every
failure preserves active state, callbacks, and the previous dirty flag.

`CustomValue::validateState(const uint8_t*, size_t) const` checks staged custom
bytes without mutating active state. The default accepts exactly `stateSize()`
bytes. A custom handler that can reject persisted or reset state must migrate
that decision from `loadState()` into `validateState()`: `false` blocks the
operation, and `true` guarantees a subsequent `loadState()` for the same bytes
succeeds and publishes them. Handlers that only require exact-length validation
need no override.

`MemoryStorage` is only a test backend; it intentionally does not survive a
power cycle.

Factory reset stages the complete default payload in this workspace, validates
custom reset bytes before `erase()`, writes the replacement record, and calls
`commit()`. Validation rejection leaves storage, live and dirty state, and
callbacks untouched. Live keymap, encoder, macro, layout, and custom state plus
callbacks are published only after commit succeeds; the validated custom
`loadState()` publication is contractually non-fallible at that point. An
adapter that promises atomic commits must keep erase and write effects
provisional until `commit()` selects the replacement record; the bundled EEPROM
adapter does not promise power-loss atomicity.

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
`layoutOptionsChanged(uint32_t)` for command changes and successful
stored/default load or factory-reset publication, `changed()` for mutable state
changes, and `bootloaderJump()` for a platform reset action.

Matrix disclosure, EEPROM reset, and bootloader jump default to disabled. Set
only the corresponding `Config` boolean after reviewing the application's
security requirements. Bootloader acceptance also requires a callback and a
successful save when state is dirty. Its callback runs only after `task()` sees
the accepted response transfer complete; direct `process()` saves or rejects
but never jumps.

## 5. Connect physical features

The protocol knows the assigned QMK/VIA 16-bit keycode. Your sketch owns switch
scanning and maps each event to:

```cpp
uint16_t code = keyboard.keycode(activeLayer, row, column);
```

Interpret the keycode with your keyboard HID, mouse, consumer, and system
control implementation. This separation lets a small macro pad and a full
keyboard share the same VIA configuration core.

Matrix scanning and keycode execution can use the built-in modules:

```cpp
#include <VIA_Matrix.h>
#include <VIA_Keyboard.h>

via::Matrix matrix(matrixConfig, matrixIO);
via::Keyboard keyboard({rows, cols}, matrix, protocol, hid, activeCodes);
keyboard.begin();
keyboard.task(millis());
```

See `docs/API.md` for the full Matrix, Keyboard, Encoder, Battery, and
SleepMgr API reference.

### ESP32 (WROOM-32 and S3) adapters

Generic ESP32 adapters under `via::esp32` (guarded by `ARDUINO_ARCH_ESP32`):

- `VIA_ESP32_GPIO.h` — Arduino `pinMode`/`digitalRead` matrix IO.
- `VIA_ESP32_NVS.h` — `via::Storage` over Preferences with a 4 KB RAM
  shadow blob; real NVS only has whole-blob `getBytes`/`putBytes`, so all
  writes are provisional until `commit()`.
- `VIA_ESP32_BLE.h` — NimBLE-Arduino 2.x BLE HID keyboard adapter using
  `NimBLEHIDDevice` and an 8-byte keyboard report. The GATT notification is
  `[modifiers, reserved, keys[0..5]]`; report ID 1 is in the Report Reference
  descriptor, not prefixed to the payload (HOGP section 4.8).
- `VIA_ESP32_BLE_ViaTransport.h` — AirVIA BLE GATT VIA transport
  (128-bit `0000FF60-...` service, FF61 data, FF62 identity) built against
  the NimBLE-Arduino 2.x API.

The `examples/ESP32_WROOM32_VIA_BLE` sketch is the reference for
ESP32-WROOM-32, which has no USB device peripheral: both VIA configuration
and typing run over AirVIA BLE. It requires NimBLE-Arduino 2.x from the
Arduino Library Manager.

The reference now allocates 16 dynamic macro slots sharing 512 bytes; all
playback lives in `via::Keyboard`, with no sketch or host-side interpreter.
Keep `protocol.task()`, `matrix.task()` and `keyboard.task()` in the main
loop. The staging workspace includes macro bytes. Exact CRC-valid v2
zero-macro records migrate without erasing keymap/encoders/layout/custom.
Back up before flashing and do not blindly roll back to a zero-macro image
after the enlarged record has been committed. See [Dynamic macros](API.md#dynamic-macros).

`BleKeyboardHID::configured()` requires a live subscriber to the HID input
report, not just an FF60 configuration central. Native-width atomic slots
share subscription state with NimBLE's callback task. This lets the library
cancel held macro modifiers if the typing host disconnects while AirVIA
remains connected. Other platform adapters still need their own physical
macro/disconnect acceptance; this is not cross-platform certification.


The shared VIA server callback restarts advertising after a connection
while below `NIMBLE_MAX_CONNECTIONS`; the sketch still configures the
initial HID1812/FF60 advertising payload. Otherwise the first connection
hides the keyboard from the second discovery flow. The WROOM sketch
also enables advertising after disconnect. Pair the keyboard in Windows
and keep that pairing when connecting AirVIA; removing it to make the
browser discover the board is not the intended workflow. Shared-server
connection/discovery proof does not replace physical typing acceptance.

The WROOM reference uses rows `{4,19,23,13,14}`, columns
`{27,16,17,18,21,22}`, and encoder pins `{33,32}`. It avoids classic ESP32
strapping GPIO0/2/5/12/15 and flash GPIO6-11. GPIO16/17 are only suitable
on WROOM variants without PSRAM. A single momentary GPIO4-to-GPIO16
jumper is sufficient for a bench keyboard smoke; production multi-key
matrix/diode acceptance is separate.

`MatrixIO::read()` returns the electrical level: HIGH is true. `Matrix`
performs the active-low conversion, so logical idle is `0`, and R0/C1
pressed is bit 1 (`0x02`), not `0x3d`.

Existing NVS keymaps are retained; correcting example default keycodes
does not replace saved assignments. Re-pair the keyboard after changing
its report descriptor/identity. The reference now uses the board's native
BLE identity instead of a shared hardcoded address.

References: [HOGP section 4.8](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/HOGP_v1.0/out/en/index-en.html)
and [ESP32 boot mode/strapping](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).


`VIA_ESP32S3_GPIO.h`, `VIA_ESP32S3_NVS.h`, and `VIA_ESP32S3_BLE.h` remain
for the S3 dual-mode sketch at `examples/ESP32S3_VIA_BLE`, which uses USB
for VIA configuration and BLE for wireless typing.
`via::esp32s3::BLEViaTransport` is an alias for the generic
`via::esp32::BLEViaTransport` implementation. Its separate HID/GPIO adapters
need independent verification; WROOM bench results do not certify S3.

#### WROOM authenticated Wi-Fi OTA

The reference sketch includes the ESP32 core's `WiFi` and `ArduinoOTA`
libraries. No credentials are compiled into the firmware. USB-flash the
OTA-capable sketch once using the default dual-application partition:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --library . --output-dir .cooper/artifacts/wroom-keyboard-ota examples/ESP32_WROOM32_VIA_BLE
arduino-cli upload --port COM39 --fqbn esp32:esp32:esp32 --upload-property upload.speed=115200 --input-dir .cooper/artifacts/wroom-keyboard-ota
python extras/wroom_ota.py provision --port COM39
```

Interactive provisioning asks for SSID and a hidden Wi-Fi password.
Automation may supply one JSON object through a non-echoed stdin pipe
with `ssid` and `password` fields; never put the JSON in source,
committed files, shell history or diagnostic logs.

The helper generates a separate strong OTA password and saves it in the
current Windows user's Credential Manager under
`AirVIA/WROOM/24-6F-28-A0-DB-34`. Override `--credential` when using another
board. `AIRVIA_OTA_PASSWORD` can instead supply a password from a protected
environment. Do not log its value or pass it on a process command line.
The ESP32 stores Wi-Fi settings and only the OTA password hash as one
Preferences blob (`airvia-ota/config`), separate from keyboard NVS.

The status output includes `[OTA] ip=...`, `[OTA] image=...` and
`[OTA] slot=...`. After provisioning, update the application by IP:

```sh
python extras/wroom_ota.py upload --ip <ESP32_IP> --image .cooper/artifacts/wroom-keyboard-ota/ESP32_WROOM32_VIA_BLE.ino.bin --espota "<INSTALLED_ESP32_CORE>/tools/espota.py"
```

Use only the application `.ino.bin`, never the merged, bootloader or
partition image. This is a command-line OTA service, not an HTTP upload
page. Keep OTA support in subsequent firmware. The default OTA slots are
1310720 bytes each; the combined keyboard/Wi-Fi build must fit one slot.
The helper's upload callback listens on host TCP3233; allow only the
ESP32's LAN IP if a scoped firewall rule is needed. ESP32 OTA invitations
use UDP3232. No COM port is opened for the IP upload.

USB `status --port COM39` can report current IP, image MD5 and active slot.
DHCP may change the IP; the sketch also advertises a unique `airvia-...`
hostname via mDNS/Arduino discovery. OTA keeps the keyboard NVS and Wi-Fi
blob; it does not preserve unsaved in-RAM edits. Release keys and save
configuration before updating. The firmware releases BLE keys at OTA start.

**Security/recovery boundary:** ArduinoOTA digest authentication is not
encrypted transport, signed firmware, secure boot, anti-rollback or an
unattended production deployment system. Use a trusted LAN, no internet
port-forwarding. NVS is not flash-encrypted by this example. Retain USB
recovery if Wi-Fi settings are wrong, credentials are lost, or an image
removes OTA support; normal USB upload does not erase keyboard/Wi-Fi NVS.
Never use an erase-all command as a recovery default.


### STM32F103 adapters

`VIA_STM32F1_GPIO.h`, `VIA_STM32F1_USB.h`, `VIA_STM32F1_Flash.h`, and
`VIA_STM32F1_Boot.h` provide Arduino GPIO matrix IO, a Cube USB Device
composite class (boot keyboard + VIA Raw HID), dual-slot atomic flash
storage, and a ROM USART bootloader coordinator. All are compile-tested
against STM32duino 3.0.0.

## BLE Transports (Bluefruit)

- **MTU:** Must negotiate a minimum MTU of 35 bytes (VIA packet + headers) for reliable AirVIA transfers.
- **Ownership:** The VIA transport expects to own the BLE stack initialization.
- **Advertising:** Must include the proper Service UUID in the advertising payload for VIA discovery.
- **Concurrency:** BLE callbacks and the main `loop()` must be protected by a mutex when accessing shared protocol state.
- **Packet Size:** Must rigorously enforce the 32-byte VIA packet size.
- **Storage:** Use a dual-slot record strategy to prevent data loss during writes. Do not auto-format corrupt filesystems.

## 7. Test in VIA

1. Use a unique USB VID/PID appropriate for your product.
2. Create a V3 definition JSON that has the same matrix rows, columns, layers,
   and custom values as the firmware.
3. Load that JSON in VIA Design tab, leaving V2 definitions disabled.
4. Verify protocol version, key read/write, macros, custom values, save,
   unplug/replug, and factory reset.
