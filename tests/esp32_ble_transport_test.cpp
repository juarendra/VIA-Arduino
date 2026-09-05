#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include "VIA_ESP32_BLE_ViaTransport.h"
#include "fakes/NimBLEFake.h"
#include "VIA_Protocol.h"

// Fake FreeRTOS mutex control (declared in the transport header).
bool g_fake_mutex_take = true;

static const char* kServiceFF60 = "0000FF60-0000-1000-8000-00805F9B34FB";

void test_initialization() {
    Nimble.reset();

    via::esp32::BLEViaTransport transport;
    assert(transport.begin("12345678901234567890123456789", 0x01020304));

    assert(Nimble.deviceName == "12345678901234567890123456789");
    assert(Nimble.initCount == 1);

    // Service UUID advertised.
    assert(!Nimble.advertisedServiceUuids.empty());
    assert(Nimble.advertisedServiceUuids[0] == kServiceFF60);
    assert(Nimble.advertisingStartCount == 1);

    NimBLECharacteristic* ff61 = Nimble.findChar("FF61");
    assert(ff61 != nullptr);
    assert(ff61->getProperties() ==
           (NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY));
    // Initial read fallback buffer: 32 zero bytes.
    assert(ff61->size() == via::kPacketSize);

    NimBLECharacteristic* ff62 = Nimble.findChar("FF62");
    assert(ff62 != nullptr);
    assert(ff62->getProperties() == NIMBLE_PROPERTY::READ);
    assert(ff62->size() == via::kPacketSize);

    NimBLEAttValue infoValue = ff62->getValue();
    const uint8_t* info = infoValue.data();
    assert(info[0] == 0x01);
    assert(info[1] == 0x02);
    assert(info[2] == 0x03);
    assert(info[3] == 0x04);
    assert(memcmp(info + 4, "1234567890123456789012345678", 28) == 0);
}

void test_write_dispatch() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    transport.begin("AirVIA", 1);

    uint8_t short_packet[31] = {1};
    uint8_t long_packet[33] = {1};
    uint8_t valid_packet[32] = {42};
    uint8_t second_valid_packet[32] = {43};

    // Dispatch invalid sizes (fake accepts all, transport rejects bad len).
    assert(Nimble.dispatchWrite(short_packet, 31) == true);
    assert(Nimble.dispatchWrite(long_packet, 33) == true);

    uint8_t received[32] = {0};
    assert(transport.receive(received) == false);  // nothing valid yet

    // Dispatch valid, then a second valid before reading.
    assert(Nimble.dispatchWrite(valid_packet, 32) == true);
    assert(Nimble.dispatchWrite(second_valid_packet, 32) == true);

    // First one remains, second is dropped.
    assert(transport.receive(received) == true);
    assert(received[0] == 42);
    assert(transport.droppedPackets() == 1);
    assert(transport.receive(received) == false);
    assert(transport.droppedPackets() == 1);
}

void test_send() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    transport.begin("AirVIA", 1);

    uint8_t packet[32] = {99};
    NimBLECharacteristic* ff61 = Nimble.findChar("FF61");

    // Disconnected.
    assert(transport.connected() == false);
    assert(transport.send(packet) == false);

    // Connected, unsubscribed: readable value updated, no notification.
    Nimble.connect();
    assert(transport.connected() == true);
    assert(transport.subscribers() == 0);
    assert(transport.send(packet) == true);
    assert(ff61->size() == via::kPacketSize);
    assert(ff61->getValue().data()[0] == 99);
    assert(Nimble.notifyCount == 0);

    // Connected, subscribed: notification delivered.
    Nimble.dispatchSubscribe(0x01);
    assert(transport.subscribers() == 1);
    assert(transport.send(packet) == true);
    assert(ff61->getValue().data()[0] == 99);
    assert(Nimble.notifyCount == 1);
    assert(Nimble.lastNotified.size() == via::kPacketSize);
    assert(Nimble.lastNotified[0] == 99);

    // Connected, subscribed, notify fails.
    Nimble.notifySuccess = false;
    assert(transport.send(packet) == false);
    assert(Nimble.notifyCount == 1);
}

void test_subscribe_tracking() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    transport.begin("AirVIA", 1);
    Nimble.connect();

    assert(transport.subscribers() == 0);
    Nimble.dispatchSubscribe(0x01);
    assert(transport.subscribers() == 1);
    Nimble.dispatchSubscribe(0x01);
    assert(transport.subscribers() == 2);
    Nimble.dispatchSubscribe(0x00);
    assert(transport.subscribers() == 1);
}

void test_short_name_zero_padding() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    NimBLECharacteristic* ff62 = Nimble.findChar("FF62");
    NimBLEAttValue infoValue = ff62->getValue();
    const uint8_t* info = infoValue.data();
    assert(memcmp(info + 4, "AirVIA", 6) == 0);
    for (size_t i = 10; i < via::kPacketSize; ++i) assert(info[i] == 0);
}

void test_lock_failure_drops_packet() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    uint8_t packet[32] = {42};
    uint8_t received[32] = {};
    g_fake_mutex_take = false;
    assert(Nimble.dispatchWrite(packet, sizeof(packet)));
    g_fake_mutex_take = true;
    assert(!transport.receive(received));
    assert(transport.droppedPackets() == 0);
}

void test_rejects_second_live_instance() {
    Nimble.reset();
    via::esp32::BLEViaTransport first;
    via::esp32::BLEViaTransport second;
    assert(first.begin("AirVIA", 1));
    assert(!second.begin("AirVIA", 1));
}

void test_disconnect_cleanup() {
    Nimble.reset();
    via::esp32::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    Nimble.connect();

    uint8_t packet[32] = {7};
    uint8_t received[32] = {};
    assert(Nimble.dispatchWrite(packet, sizeof(packet)));
    assert(transport.receive(received) == true);
    assert(received[0] == 7);

    // A command captured from a dead connection must not survive reconnect.
    assert(Nimble.dispatchWrite(packet, sizeof(packet)));
    Nimble.disconnect();
    assert(transport.receive(received) == false);
    Nimble.connect();
    Nimble.dispatchSubscribe(0x01);
    assert(transport.send(packet) == true);
    assert(Nimble.notifyCount == 1);
    assert(Nimble.lastNotified[0] == 7);
}

int main() {
    test_initialization();
    test_short_name_zero_padding();
    test_lock_failure_drops_packet();
    test_rejects_second_live_instance();
    test_write_dispatch();
    test_send();
    test_subscribe_tracking();
    test_disconnect_cleanup();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
