#pragma once
// Fake of the real espressif/arduino-esp32 Preferences (NVS) API for native
// tests. The real API only offers 3-argument getBytes/putBytes
// (no offset parameter); blobs are stored per (namespace, key) pair and
// persist across Preferences instances for the lifetime of the process.

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

class Preferences {
 public:
  Preferences() : open_(false) {}

  bool begin(const char* name, bool readonly) {
    (void)readonly;
    if (name == nullptr) return false;
    ns_ = name;
    open_ = true;
    return true;
  }

  bool end() {
    open_ = false;
    return true;
  }

  void clear() {
    if (!open_) return;
    std::map<std::string, std::vector<uint8_t> >& s = store();
    for (std::map<std::string, std::vector<uint8_t> >::iterator it =
             s.begin();
         it != s.end();) {
      if (isNsKey(it->first)) {
        it = s.erase(it);
      } else {
        ++it;
      }
    }
  }

  size_t getBytes(const char* key, uint8_t* buf, size_t maxLen) {
    if (!open_ || key == nullptr || buf == nullptr) return 0;
    const std::map<std::string, std::vector<uint8_t> >& s = store();
    std::map<std::string, std::vector<uint8_t> >::const_iterator it =
        s.find(makeKey(key));
    if (it == s.end()) return 0;
    size_t n = it->second.size() < maxLen ? it->second.size() : maxLen;
    for (size_t i = 0; i < n; ++i) buf[i] = it->second[i];
    return n;
  }

  size_t putBytes(const char* key, const uint8_t* value, size_t len) {
    if (!open_ || key == nullptr || (value == nullptr && len > 0)) return 0;
    store()[makeKey(key)].assign(value, value + len);
    return len;
  }

  size_t getLength(const char* key) {
    if (!open_ || key == nullptr) return 0;
    const std::map<std::string, std::vector<uint8_t> >& s = store();
    std::map<std::string, std::vector<uint8_t> >::const_iterator it =
        s.find(makeKey(key));
    return it == s.end() ? 0 : it->second.size();
  }

 private:
  static std::map<std::string, std::vector<uint8_t> >& store() {
    static std::map<std::string, std::vector<uint8_t> > s;
    return s;
  }

  std::string makeKey(const char* key) const {
    return ns_ + std::string("\0", 1) + key;
  }

  bool isNsKey(const std::string& stored) const {
    if (stored.size() <= ns_.size()) return false;
    if (stored.compare(0, ns_.size(), ns_) != 0) return false;
    return stored[ns_.size()] == '\0';
  }

  std::string ns_;
  bool open_;
};
