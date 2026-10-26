#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "VIA_ESP32_BLE.h"
#include "VIA_ESP32_BLE_ViaTransport.h"
#include "VIA_ESP32_GPIO.h"
#include "VIA_Protocol.h"
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

void test_wroom_shared_server_full_flow(bool viaFirst) {
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
  assert(adv->isAdvertising());  // First peer must not hide the other service.
  if (viaFirst) Nimble.dispatchSubscribe(0x01);
  else Nimble.dispatchHidSubscribe(1, 0);
  Nimble.connect();
  assert(adv->isAdvertising());
  if (!viaFirst) Nimble.dispatchSubscribe(0x01);
  else Nimble.dispatchHidSubscribe(1, 1);
  assert(hid.configured() == true);
  assert(transport.connected() == true);

  // HID report over the shared connection.
  via::KeyboardReport report = {};
  report.modifiers = 0x01;
  report.keys[0] = 0x04;
  assert(hid.send(report));
  assert(Nimble.notifyCount == 1);
  assert(Nimble.lastNotified ==
         std::vector<uint8_t>({0x01, 0x00, 0x04, 0, 0, 0, 0, 0}));

  // AirVIA request/response over the same shared connection.
  assert(server->getConnectedCount() == 2);
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

  while (server->getConnectedCount() < NIMBLE_MAX_CONNECTIONS) {
    Nimble.connect();
    assert(adv->isAdvertising() ==
           (server->getConnectedCount() < NIMBLE_MAX_CONNECTIONS));
  }
  assert(hid.send(via::KeyboardReport{}));
  assert(Nimble.lastNotified == std::vector<uint8_t>(8, 0));

  // Disconnect cleanup on the shared server.
  uint8_t pending[32] = {0xBB};
  assert(Nimble.dispatchWrite(pending, sizeof(pending)));
  Nimble.disconnect();
  assert(!transport.receive(received));
}

void test_gpio_press_release_reaches_hid() {
  Nimble.reset();
  NimBLEDevice::init("AirVIA WROOM32");
  NimBLEServer* server = NimBLEDevice::createServer();
  via::esp32::BleKeyboardHID hid;
  via::esp32::BLEViaTransport transport;
  assert(hid.begin(server, "AirVIA WROOM32", "AirVIA"));
  assert(transport.begin(server, "AirVIA WROOM32", 1));
  server->start();
  Nimble.connect();
  Nimble.dispatchHidSubscribe(1);

  const via::Pin rows[] = {4}, columns[] = {16};
  uint32_t raw[1] = {}, candidate[1] = {}, stable[1] = {}, changed[1] = {};
  via::esp32::MatrixIOArduino io;
  via::MatrixConfig matrixConfig = {1, 1, rows, columns, via::kColToRow, 30, 5,
                                    raw, candidate, stable, changed};
  via::Matrix matrix(matrixConfig, io);
  uint16_t keymap[1] = {}, activeCodes[1] = {};
  const uint16_t defaults[] = {0x001A};  // W
  via::Config config(1, 1, 1, keymap, defaults);
  via::Protocol protocol(config, transport);
  via::Keyboard keyboard({1, 1}, matrix, protocol, hid, activeCodes);
  assert(matrix.begin() && protocol.begin() && keyboard.begin());

  Nimble.dispatchSubscribe(0x01);
  uint8_t request[32] = {0x01};
  assert(Nimble.dispatchWrite(request, sizeof(request)));
  protocol.task(0);
  assert(transport.connected() && transport.subscribers() == 1);
  assert(Nimble.lastNotified.size() == 32);  // VIA reply uses the same link.
  const int viaReplies = Nimble.notifyCount;

  FakeArduino::value[16] = HIGH;
  matrix.task(0); keyboard.task(0);
  matrix.task(5); keyboard.task(5);
  assert(Nimble.notifyCount == viaReplies);  // An open input must not type.

  Nimble.notifySuccess = false;
  FakeArduino::value[16] = LOW;
  matrix.task(10); keyboard.task(10);
  matrix.task(15); keyboard.task(15);
  assert(Nimble.notifyCount == viaReplies);

  // A rejected notification must recover without another physical key edge.
  Nimble.notifySuccess = true;
  matrix.task(20); keyboard.task(20);
  assert(Nimble.lastNotified ==
         std::vector<uint8_t>({0, 0, 0x1A, 0, 0, 0, 0, 0}));

  Nimble.notifySuccess = false;
  FakeArduino::value[16] = HIGH;
  matrix.task(25); keyboard.task(25);
  matrix.task(30); keyboard.task(30);
  assert(Nimble.lastNotified[2] == 0x1A);  // Host has not received key-up yet.

  Nimble.notifySuccess = true;
  matrix.task(35); keyboard.task(35);
  assert(Nimble.lastNotified == std::vector<uint8_t>(8, 0));
  assert(transport.connected() && transport.subscribers() == 1);
}

void test_macro_cancels_when_hid_peer_leaves_configuration_peer() {
  Nimble.reset();
  NimBLEDevice::init("AirVIA WROOM32");
  NimBLEServer* server = NimBLEDevice::createServer();
  via::esp32::BleKeyboardHID hid;
  via::esp32::BLEViaTransport transport;
  assert(hid.begin(server, "AirVIA WROOM32", "AirVIA"));
  assert(transport.begin(server, "AirVIA WROOM32", 1));
  server->start();
  Nimble.connect();
  Nimble.dispatchSubscribe(1);
  Nimble.connect();
  Nimble.dispatchHidSubscribe(1, 1);

  const via::Pin rows[] = {4}, columns[] = {16};
  uint32_t raw[1] = {}, candidate[1] = {}, stable[1] = {}, changed[1] = {};
  via::esp32::MatrixIOArduino io;
  via::MatrixConfig matrixConfig = {1, 1, rows, columns, via::kColToRow, 30, 5,
                                    raw, candidate, stable, changed};
  via::Matrix matrix(matrixConfig, io);
  uint16_t keys[1] = {}, active[1] = {};
  const uint16_t defaults[] = {0x7700};
  uint8_t macros[64] = {};
  via::Config config(1, 1, 1, keys, defaults, macros, sizeof(macros), 1);
  via::Protocol protocol(config, transport);
  via::Keyboard keyboard({1, 1}, matrix, protocol, hid, active);
  assert(matrix.begin() && protocol.begin() && keyboard.begin());
  uint8_t packet[32] = {0x0F, 0, 0, 13, 1, 2, 0xE0, 1, 4,
                        '1', '0', '0', '|', 1, 1, 0x06, 0};
  assert(protocol.process(packet, 0));
  FakeArduino::value[16] = HIGH;
  for (uint32_t now = 0; now < 10; ++now) {
    matrix.task(now); keyboard.task(now);
  }
  FakeArduino::value[16] = LOW;
  for (uint32_t now = 10; now < 30; ++now) {
    matrix.task(now); keyboard.task(now);
  }
  assert(keyboard.macroPlaying() && Nimble.lastNotified[0] == 1);
  Nimble.disconnectPeer(1);
  assert(server->getConnectedCount() == 1 && !hid.configured());
  matrix.task(30); keyboard.task(30);
  assert(!keyboard.macroPlaying());
  Nimble.connect();
  Nimble.dispatchHidSubscribe(1, 1);
  for (uint32_t now = 31; now < 45; ++now) {
    matrix.task(now); keyboard.task(now);
  }
  assert(!keyboard.macroPlaying());  // A held M0 never replays after reconnect.
  assert(Nimble.lastNotified == std::vector<uint8_t>(8, 0));
}

int main() {
  test_wroom_shared_server_full_flow(false);  // Windows/HID first.
  test_wroom_shared_server_full_flow(true);   // AirVIA first.
  test_gpio_press_release_reaches_hid();
  test_macro_cancels_when_hid_peer_leaves_configuration_peer();
  std::cout << "All tests passed!" << std::endl;
  return 0;
}