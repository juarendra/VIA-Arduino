import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WROOM = ROOT / "examples" / "ESP32_WROOM32_VIA_BLE" / "ESP32_WROOM32_VIA_BLE.ino"
S3 = ROOT / "examples" / "ESP32S3_VIA_BLE" / "ESP32S3_VIA_BLE.ino"


def fail(message):
    print(message)
    sys.exit(1)


def count(pattern, text):
    return len(re.findall(pattern, text))


def check(condition, message):
    if not condition:
        fail(message)


for path in (WROOM, S3):
    check(path.exists(), f"missing example: {path}")

wroom = WROOM.read_text(encoding="utf-8")
s3 = S3.read_text(encoding="utf-8")

for name, text in (("WROOM", wroom), ("S3", s3)):
    check(count(r"NimBLEDevice::init\(", text) == 1,
          f"{name}: expected exactly one NimBLEDevice::init")
    check(count(r"NimBLEDevice::createServer\(", text) == 1,
          f"{name}: expected exactly one NimBLEDevice::createServer")
    check(count(r"bleHid\.begin\(server", text) == 1,
          f"{name}: expected bleHid.begin(server, ...)")
    check(count(r"bleVia\.begin\(server", text) == 1,
          f"{name}: expected bleVia.begin(server, ...)")
    check(count(r"server->start\(\)", text) == 1,
          f"{name}: expected exactly one server->start()")
    check(count(r"getAdvertising\(\)", text) == 1,
          f"{name}: expected exactly one getAdvertising()")
    check(count(r"adv->start\(\)", text) == 1,
          f"{name}: expected exactly one adv->start()")
    check("0x1812" in text, f"{name}: missing HID 0x1812 advertising UUID")
    check("0000FF60-0000-1000-8000-00805F9B34FB" in text,
          f"{name}: missing AirVIA FF60 advertising UUID")
    check("TinyUSB" not in text, f"{name}: must not require TinyUSB")
    check("Adafruit" not in text, f"{name}: must not require Adafruit")

check("via::esp32s3::BleKeyboardHID" in s3,
      "S3: missing via::esp32s3::BleKeyboardHID alias usage")
check("via::esp32s3::BLEViaTransport" in s3,
      "S3: missing via::esp32s3::BLEViaTransport alias usage")

print("ESP32 shared-server contract tests passed")