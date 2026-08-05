from pathlib import Path

source = Path(
    "examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.ino"
).read_text(encoding="utf-8")

for forbidden in (
    "Adafruit_TinyUSB.h",
    "VIA_TinyUSB_RawHID.h",
    "VIA_TinyUSB_Keyboard.h",
    "viaRawHid",
    "usbKeyboard",
    "blePacket",
    "protocol.process(",
):
    assert forbidden not in source, forbidden

for required in (
    "static uint8_t storageStaging[4096]",
    "via::Protocol protocol(protocolConfig, bleVia, &storage);",
    "Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);",
    "if (!Bluefruit.begin(1, 0)) return;",
    "Bluefruit.Security.setIOCaps(false, false, false);",
    "bleHidSvc.begin();",
    "if (!bleHid.begin()) return;",
    'if (!bleVia.begin("AirVIA nice!nano", 0x00000001)) return;',
    "if (!storage.begin()) return;",
    "if (!protocol.begin(millis())) return;",
    "if (!keyboard.begin()) return;",
    "startAdvertising();",
):
    assert required in source, required

order = (
    "Bluefruit.configPrphBandwidth(BANDWIDTH_HIGH);",
    "if (!Bluefruit.begin(1, 0)) return;",
    "bleHidSvc.begin();",
    "if (!bleHid.begin()) return;",
    'if (!bleVia.begin("AirVIA nice!nano", 0x00000001)) return;',
    "if (!storage.begin()) return;",
    "if (!protocol.begin(millis())) return;",
    "if (!keyboard.begin()) return;",
    "startAdvertising();",
)
positions = [source.index(item) for item in order]
assert positions == sorted(positions)
