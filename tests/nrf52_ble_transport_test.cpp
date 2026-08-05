#include <cassert>
#include <iostream>
#include <cstring>
#include "VIA_nRF52_BLE_ViaTransport.h"
#include "fakes/bluefruit.h"
#include "VIA_Protocol.h"

// Define static variables of FakeBluefruit here for tests
extern FakeBluefruit Bluefruit;
bool g_fake_mutex_take = true;

void test_initialization() {
    FakeBluefruit::reset();
    
    via::nrf52::BLEViaTransport transport;
    assert(transport.begin("12345678901234567890123456789", 0x01020304));
    
    assert(FakeBluefruit::serviceUuid == 0xFF60);
    assert(FakeBluefruit::ff61Uuid == 0xFF61);
    assert(FakeBluefruit::ff61FixedLength == via::kPacketSize);
    assert(FakeBluefruit::ff62Uuid == 0xFF62);
    
    assert(FakeBluefruit::ff62Value[0] == 0x01);
    assert(FakeBluefruit::ff62Value[1] == 0x02);
    assert(FakeBluefruit::ff62Value[2] == 0x03);
    assert(FakeBluefruit::ff62Value[3] == 0x04);
    
    assert(FakeBluefruit::ff61Properties ==
           (CHR_PROPS_READ | CHR_PROPS_WRITE |
            CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY));
    assert(FakeBluefruit::ff61ReadPermission == SECMODE_OPEN);
    assert(FakeBluefruit::ff61WritePermission == SECMODE_OPEN);
    assert(FakeBluefruit::ff62Properties == CHR_PROPS_READ);
    assert(FakeBluefruit::ff62ReadPermission == SECMODE_OPEN);
    assert(FakeBluefruit::ff62WritePermission == SECMODE_NO_ACCESS);
    assert(FakeBluefruit::ff62FixedLength == via::kPacketSize);
    assert(memcmp(FakeBluefruit::ff62Value + 4,
                  "1234567890123456789012345678", 28) == 0);
}

void test_write_dispatch() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    transport.begin("AirVIA", 1);
    
    uint8_t short_packet[31] = {1};
    uint8_t long_packet[33] = {1};
    uint8_t valid_packet[32] = {42};
    uint8_t second_valid_packet[32] = {43};
    
    // Dispatch invalid sizes
    assert(FakeBluefruit::dispatchWrite(short_packet, 31) == false);
    assert(FakeBluefruit::dispatchWrite(long_packet, 33) == false);
    
    uint8_t received[32] = {0};
    assert(transport.receive(received) == false); // Should have nothing
    
    // Dispatch valid
    assert(FakeBluefruit::dispatchWrite(valid_packet, 32) == true);
    
    // Dispatch second valid before reading
    assert(FakeBluefruit::dispatchWrite(second_valid_packet, 32) == true);
    
    // First one remains, dropped count goes up
    assert(transport.receive(received) == true);
    assert(received[0] == 42);
    assert(transport.droppedPackets() == 1);
}

void test_send() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    transport.begin("AirVIA", 1);
    
    uint8_t packet[32] = {99};
    
    // Disconnected
    Bluefruit.connectedResult = false;
    assert(transport.send(packet) == false);
    
    // Connected, unsubscribed (read fallback)
    Bluefruit.connectedResult = true;
    FakeBluefruit::ff61Notifying = false;
    assert(transport.send(packet) == true);
    assert(FakeBluefruit::ff61Written[0] == 99); // Fallback write happened
    assert(FakeBluefruit::ff61NotifyCount == 0);
    
    // Connected, subscribed (notification)
    FakeBluefruit::ff61Written[0] = 0; // Clear fallback state
    FakeBluefruit::ff61Notifying = true;
    FakeBluefruit::ff61NotifySuccess = true;
    assert(transport.send(packet) == true);
    assert(FakeBluefruit::ff61Written[0] == 99); // Fallback write always happens first
    assert(FakeBluefruit::ff61NotifyCount == 1);
    
    // Connected, subscribed, notify fails
    FakeBluefruit::ff61NotifySuccess = false;
    assert(transport.send(packet) == false);
}

void test_short_name_zero_padding() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    assert(memcmp(FakeBluefruit::ff62Value + 4, "AirVIA", 6) == 0);
    for (size_t i = 10; i < 32; ++i) assert(FakeBluefruit::ff62Value[i] == 0);
}

void test_lock_failure_drops_packet() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport transport;
    assert(transport.begin("AirVIA", 1));
    uint8_t packet[32] = {42};
    uint8_t received[32] = {};
    g_fake_mutex_take = false;
    assert(FakeBluefruit::dispatchWrite(packet, sizeof(packet)));
    g_fake_mutex_take = true;
    assert(!transport.receive(received));
}

void test_rejects_second_live_instance() {
    FakeBluefruit::reset();
    via::nrf52::BLEViaTransport first;
    via::nrf52::BLEViaTransport second;
    assert(first.begin("AirVIA", 1));
    assert(!second.begin("AirVIA", 1));
}

int main() {
    test_initialization();
    test_short_name_zero_padding();
    test_lock_failure_drops_packet();
    test_rejects_second_live_instance();
    test_write_dispatch();
    test_send();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
