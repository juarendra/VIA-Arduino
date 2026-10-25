#include <cassert>
#include <cstring>
#include <string>
#include "../examples/ESP32_WROOM32_VIA_BLE/WroomOtaConfig.h"

int main() {
  const std::string hash(32, 'a');
  const std::string good = "OTA_SETUP\tbench-network\tbench-only-pass\t" + hash;
  wroom_ota::Config config = {};
  assert(wroom_ota::parseSetup(good.data(), good.size(), config));
  assert(std::strcmp(config.ssid, "bench-network") == 0);
  assert(std::strcmp(config.password, "bench-only-pass") == 0);
  assert(std::strcmp(config.otaHash, hash.c_str()) == 0);
  const wroom_ota::Config previous = config;
  const std::string bad[] = {
      "OTA_SETUP\t\tbench-only-pass\t" + hash,
      "OTA_SETUP\t" + std::string(33, 's') + "\tbench-only-pass\t" + hash,
      "OTA_SETUP\tbench\tshort\t" + hash,
      "OTA_SETUP\tbench\t" + std::string(64, 'z') + "\t" + hash,
      "OTA_SETUP\tbench\tbench-only-pass\t" + std::string(32, 'z'),
      good + "\textra", good.substr(0, good.size() - 1),
      std::string(161, 'x'), std::string("OTA_SETUP\tbench\0hidden", 22),
  };
  for (const auto& line : bad) {
    assert(!wroom_ota::parseSetup(line.data(), line.size(), config));
    assert(std::memcmp(&previous, &config, sizeof(config)) == 0);
  }
  const std::string maximum = "OTA_SETUP\t" + std::string(32, 's') + "\t" +
                              std::string(64, 'b') + "\t" + hash;
  assert(wroom_ota::parseSetup(maximum.data(), maximum.size(), config));
  std::memset(config.ssid, 'x', sizeof(config.ssid));
  assert(!wroom_ota::valid(config));
}
