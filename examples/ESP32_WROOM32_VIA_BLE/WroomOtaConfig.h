#pragma once

#include <stddef.h>
#include <string.h>

namespace wroom_ota {

struct Config {
  char ssid[33];
  char password[65];
  char otaHash[33];
};

inline bool hexString(const char* value, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

inline bool valid(const Config& config) {
  if (!memchr(config.ssid, 0, sizeof(config.ssid)) ||
      !memchr(config.password, 0, sizeof(config.password)) ||
      !memchr(config.otaHash, 0, sizeof(config.otaHash))) return false;
  const size_t ssidLength = strlen(config.ssid);
  const size_t passwordLength = strlen(config.password);
  if (!ssidLength || passwordLength < 8 || strlen(config.otaHash) != 32)
    return false;
  const char* fields[] = {config.ssid, config.password};
  for (const char* field : fields) {
    for (const char* p = field; *p; ++p) {
      if (static_cast<unsigned char>(*p) < 32 || *p == 127) return false;
    }
  }
  return hexString(config.otaHash, 32) &&
         (passwordLength != 64 || hexString(config.password, 64));
}

inline bool parseSetup(const char* line, size_t length, Config& output) {
  const char prefix[] = "OTA_SETUP\t";
  if (length > 160 || length < sizeof(prefix) || memchr(line, 0, length) ||
      memcmp(line, prefix, sizeof(prefix) - 1) != 0) return false;
  char buffer[161];
  memcpy(buffer, line, length);
  buffer[length] = 0;
  char* ssid = buffer + sizeof(prefix) - 1;
  char* password = strchr(ssid, '\t');
  if (!password) return false;
  *password++ = 0;
  char* hash = strchr(password, '\t');
  if (!hash) return false;
  *hash++ = 0;
  if (strlen(ssid) > 32 || strlen(password) > 64 || strlen(hash) != 32)
    return false;
  Config candidate = {};
  memcpy(candidate.ssid, ssid, strlen(ssid));
  memcpy(candidate.password, password, strlen(password));
  memcpy(candidate.otaHash, hash, 32);
  if (!valid(candidate)) return false;
  output = candidate;
  memset(buffer, 0, sizeof(buffer));
  return true;
}

}  // namespace wroom_ota
