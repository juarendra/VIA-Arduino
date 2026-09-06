#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "VIA_ESP32_BLE.h"
#include "VIA_ESP32_BLE_ViaTransport.h"
#include "fakes/NimBLEFake.h"

// Fake FreeRTOS mutex control (declared in the transport header).
bool g_fake_mutex_take = true;

static bool advertisedIncludes(const std::vector<std::string>& uuids,
                               const char* suffix) {
  for (size_t i = 0; i < uuids.size(); ++i) {
    if (uuids[i].find(suffix) != std::string::npos) return true;
  }
  return false;
}

void test_wroom_shared_server_full_flow() {
  Nimble.reset();
  assert(NimBLEDevice::init("AirVIA WROOM32"));
  NimBLEServer* server = NimBLEDevice::createServer();
  assert(server != nullptr);
  assert(Nimble.createServerCount == 1);

  via::esp32::BleKeyboardHID hid;
  via::esp32::BLEViaTransport transport;
  assert(hid.begin(server, "AirVIA WROOM32", "AirVIA"));
  assert(transport.begin(server, "AirVIA WROOM32", 0x00000001));

  // One shared server, two services, no hidden server/start/advertising.
  assert(Nimble.createServerCount == 1);
  assert(Nimble.serverStartCount == 0);
  assert(Nimble.advertisingStartCount == 0);
  assert(server->services().size() == 2);
  NimBLEService* hidService = Nimble.findService("0x1812");
  NimBLEService* viaService = Nimble.findService("FF60");
  assert(hidService != nullptr);
  assert(viaService != nullptr);
  assert(hidService->server() == server);
  assert(viaService->server() == server);

  server->start();
  assert(Nimble.serverStartCount == 1);
  NimBLEAdvertising* adv = server->getAdvertising();
  assert(adv != nullptr);
  adv->addServiceUUID(NimBLEUUID("0x1812"));
  adv->addServiceUUID(NimBLEUUID(
      "0000FF60-0000-1000-8000-00805F9B34FB"));
  adv->setName("AirVIA WROOM32");
  adv->start();
  assert(Nimble.advertisingStartCount == 1);
  assert(advertisedIncludes(Nimble.advertisedServiceUuids, "0x1812"));
  assert(
      advertisedIncludes(Nimble.advertisedServiceUuids,
                         "0000FF60-0000-1000-8000-00805F9B34FB"));

  Nimble.connect();
  assert(hid.configured() == true);
  assert(transport.connected() == true);

  // HID report over the shared connection.
  via::KeyboardReport report = {};
  report.modifiers = 0x01;
  report.keys[0] = 0x04;
  assert(hid.send(report));
  assert(Nimble.notifyCount == 1);
  assert(Nimble.lastNotified.size() == 9);
  assert(Nimble.lastNotified[0] == 1);
  assert(Nimble.lastNotified[1] == 0x01);
  assert(Nimble.lastNotified[3] == 0x04);

  // AirVIA request/response over the same shared connection.
  Nimble.dispatchSubscribe(0x01);
  assert(transport.subscribers() == 1);

  const int notifyBefore = Nimble.notifyCount;
  uint8_t request[32] = {};
  request[0] = 0x01;
  assert(Nimble.dispatchWrite(request, sizeof(request)));
  uint8_t received[32] = {};
  assert(transport.receive(received));
  assert(received[0] == 0x01);

  uint8_t response[32] = {0xAA};
  assert(transport.send(response));
  assert(Nimble.notifyCount == notifyBefore + 1);
  assert(Nimble.lastNotified.size() == 32);
  assert(Nimble.lastNotified[0] == 0xAA);

  // Disconnect cleanup on the shared server.
  uint8_t pending[32] = {0xBB};
  assert(Nimble.dispatchWrite(pending, sizeof(pending)));
  Nimble.disconnect();
  assert(!transport.receive(received));
}

int main() {
  test_wroom_shared_server_full_flow();
  std::cout << "All tests passed!" << std::endl;
  return 0;
}