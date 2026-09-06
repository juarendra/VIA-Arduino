#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "VIA_ESP32S3_BLE.h"
#include "fakes/NimBLEFake.h"

// The S3 HID adapter must create its services on the shared server before the
// transport starts it, advertise the HID (0x1812) service and the device name,
// and reject reports while no central is connected.
void test_hid_begin_advertises_keyboard() {
    Nimble.reset();
    assert(NimBLEDevice::init("AirVIA S3"));
    assert(NimBLEDevice::createServer() != nullptr);

    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin("AirVIA S3", "AirVIA"));

    bool sawHid = false;
    for (size_t i = 0; i < Nimble.advertisedServiceUuids.size(); ++i) {
        if (Nimble.advertisedServiceUuids[i].find("0x1812") !=
            std::string::npos)
            sawHid = true;
    }
    assert(sawHid);

    assert(Nimble.advertisingObj.name() == "AirVIA S3");
}

void test_hid_unconnected_rejects() {
    Nimble.reset();
    NimBLEDevice::init("AirVIA S3");
    NimBLEDevice::createServer();
    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin("AirVIA S3", "AirVIA"));

    via::KeyboardReport r = {};
    r.modifiers = 0x02;
    r.keys[0] = 0x04;

    assert(hid.configured() == false);
    assert(hid.suspended() == true);
    assert(hid.send(r) == false);
    assert(Nimble.notifyCount == 0);
}

void test_hid_connected_notifies_report() {
    Nimble.reset();
    NimBLEDevice::init("AirVIA S3");
    NimBLEDevice::createServer();
    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin("AirVIA S3", "AirVIA"));

    Nimble.connect();
    assert(hid.configured() == true);
    assert(hid.suspended() == false);

    via::KeyboardReport r = {};
    r.modifiers = 0x01;  // left control
    r.keys[0] = 0x04;    // A
    r.keys[1] = 0x05;    // S
    r.keys[5] = 0x1E;    // 1

    assert(hid.send(r) == true);

    // 9-byte notify payload: report ID + 8-byte boot-keyboard report
    // [modifiers, reserved, keys[0..5]].
    const uint8_t expected[9] = {
        1,    0x01, 0x00, 0x04, 0x05, 0x00, 0x00, 0x00, 0x1E
    };
    assert(Nimble.notifyCount == 1);
    assert(Nimble.lastNotified.size() == 9);
    for (int i = 0; i < 9; ++i) {
        assert(Nimble.lastNotified[i] == expected[i]);
    }
}

int main() {
    test_hid_begin_advertises_keyboard();
    test_hid_unconnected_rejects();
    test_hid_connected_notifies_report();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
