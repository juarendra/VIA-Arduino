#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "VIA_ESP32S3_BLE.h"
#include "fakes/NimBLEFake.h"

// The S3 HID adapter must attach its service to the shared server supplied by
// the sketch, must not create/start the server, and must reject reports while
// no central is connected.
static NimBLEServer* makeServer() {
    assert(NimBLEDevice::init("AirVIA S3"));
    NimBLEServer* server = NimBLEDevice::createServer();
    assert(server != nullptr);
    assert(Nimble.createServerCount == 1);
    return server;
}

void test_hid_begin_rejects_null_server() {
    Nimble.reset();
    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin(nullptr, "AirVIA S3", "VIA-Arduino") == false);
}

void test_hid_begin_attaches_shared_service() {
    Nimble.reset();
    NimBLEServer* server = makeServer();

    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin(server, "AirVIA S3", "VIA-Arduino"));

    assert(Nimble.createServerCount == 1);
    assert(Nimble.serverStartCount == 0);
    assert(Nimble.advertisingStartCount == 0);

    NimBLEService* hidService = Nimble.findService("0x1812");
    assert(hidService != nullptr);
    assert(hidService->server() == server);

    NimBLECharacteristic* reportMap = Nimble.findChar("0x2A49");
    assert(reportMap != nullptr);
    assert(Nimble.serviceForChar(reportMap) == hidService);
    assert(reportMap->size() > 0);

    NimBLECharacteristic* inputReport = Nimble.findChar("0x2A4D");
    assert(inputReport != nullptr);
    assert(Nimble.serviceForChar(inputReport) == hidService);
    assert(inputReport->getProperties() == NIMBLE_PROPERTY::NOTIFY);
}

void test_hid_unconnected_rejects() {
    Nimble.reset();
    NimBLEServer* server = makeServer();
    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin(server, "AirVIA S3", "VIA-Arduino"));

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
    NimBLEServer* server = makeServer();
    via::esp32s3::BleKeyboardHID hid;
    assert(hid.begin(server, "AirVIA S3", "VIA-Arduino"));

    server->start();
    Nimble.connect();
    assert(hid.configured() == true);
    assert(hid.suspended() == false);

    via::KeyboardReport r = {};
    r.modifiers = 0x01;  // left control
    r.keys[0] = 0x04;    // A
    r.keys[1] = 0x05;    // S
    r.keys[5] = 0x1E;    // 1

    assert(hid.send(r) == true);

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
    test_hid_begin_rejects_null_server();
    test_hid_begin_attaches_shared_service();
    test_hid_unconnected_rejects();
    test_hid_connected_notifies_report();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}