#include <cassert>
#include <cstring>
#include <iostream>

#include "VIA_ESP32_NVS.h"
#include <Preferences.h>

namespace {

// Seed a raw blob into the fake NVS store the way real flash would contain
// it (e.g. from a previous firmware version with a smaller payload).
void seedBlob(const char* ns, const uint8_t* data, size_t len) {
  Preferences prefs;
  assert(prefs.begin(ns, true));
  assert(prefs.putBytes("via_payload", data, len) == len);
  prefs.end();
}

}  // namespace

void test_fresh_state() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  assert(storage.capacity() == via::esp32::NVSStorage::kCapacity);
  uint8_t buf[32];
  memset(buf, 0, sizeof(buf));
  assert(storage.read(0, buf, sizeof(buf)));
  for (size_t i = 0; i < sizeof(buf); ++i) assert(buf[i] == 0xFF);
}

void test_roundtrip() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  uint8_t data[100];
  for (size_t i = 0; i < sizeof(data); ++i) data[i] = static_cast<uint8_t>(i);
  assert(storage.write(0, data, sizeof(data)));
  assert(storage.commit());

  // A "fresh boot" reads the committed bytes back.
  via::esp32::NVSStorage rebooted;
  assert(rebooted.begin("via"));
  uint8_t out[100];
  memset(out, 0, sizeof(out));
  assert(rebooted.read(0, out, sizeof(out)));
  assert(memcmp(out, data, sizeof(data)) == 0);

  // Everything after the payload stays erased.
  uint8_t tail[16];
  assert(rebooted.read(100, tail, sizeof(tail)));
  for (size_t i = 0; i < sizeof(tail); ++i) assert(tail[i] == 0xFF);
}

void test_truncated_blob_starts_erased() {
  uint8_t short_blob[10];
  memset(short_blob, 0x5A, sizeof(short_blob));
  seedBlob("via", short_blob, sizeof(short_blob));

  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  uint8_t buf[16];
  assert(storage.read(0, buf, sizeof(buf)));
  for (size_t i = 0; i < sizeof(buf); ++i) assert(buf[i] == 0xFF);
}

void test_missing_blob_starts_erased() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("other-ns"));
  uint8_t buf[16];
  assert(storage.read(0, buf, sizeof(buf)));
  for (size_t i = 0; i < sizeof(buf); ++i) assert(buf[i] == 0xFF);
}

void test_bounds() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  const size_t cap = storage.capacity();
  uint8_t one = 0x77;
  uint8_t out[2];

  // Last single byte fits.
  assert(storage.write(cap - 1, &one, 1));
  // Two bytes from the last offset overflow.
  assert(!storage.write(cap - 1, &one, 2));
  // Offset at capacity overflows for any nonzero length.
  assert(!storage.write(cap, &one, 1));
  // Zero-length operations are always fine.
  assert(storage.read(cap, out, 0));
  assert(!storage.read(cap, out, 1));
  assert(storage.write(cap, &one, 0));
  assert(storage.read(0, out, 0));

  // The in-bounds write survived.
  assert(storage.read(cap - 1, out, 1));
  assert(out[0] == 0x77);
}

void test_erase_is_provisional() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  uint8_t committed[16];
  memset(committed, 0x11, sizeof(committed));
  assert(storage.write(0, committed, sizeof(committed)));
  assert(storage.commit());

  // Erase + write new state, but power dies before commit.
  assert(storage.erase());
  uint8_t fresh[16];
  memset(fresh, 0x22, sizeof(fresh));
  assert(storage.write(0, fresh, sizeof(fresh)));

  via::esp32::NVSStorage rebooted;
  assert(rebooted.begin("via"));
  uint8_t out[16];
  assert(rebooted.read(0, out, sizeof(out)));
  assert(memcmp(out, committed, sizeof(committed)) == 0);
}

void test_erase_commit_is_durable() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  uint8_t old[16];
  memset(old, 0x33, sizeof(old));
  assert(storage.write(0, old, sizeof(old)));
  assert(storage.commit());

  assert(storage.erase());
  assert(storage.commit());

  via::esp32::NVSStorage rebooted;
  assert(rebooted.begin("via"));
  uint8_t out[16];
  assert(rebooted.read(0, out, sizeof(out)));
  for (size_t i = 0; i < sizeof(out); ++i) assert(out[i] == 0xFF);
}

void test_partial_write_is_provisional() {
  via::esp32::NVSStorage storage;
  assert(storage.begin("via"));
  uint8_t original[32];
  for (size_t i = 0; i < sizeof(original); ++i)
    original[i] = static_cast<uint8_t>(0x40 + i);
  assert(storage.write(0, original, sizeof(original)));
  assert(storage.commit());

  storage.write(12, reinterpret_cast<const uint8_t*>("\xDE\xAD\xBE\xEF"), 4);
  // No commit: the new instance must still see the original bytes.
  via::esp32::NVSStorage rebooted;
  assert(rebooted.begin("via"));
  uint8_t out[32];
  assert(rebooted.read(0, out, sizeof(out)));
  assert(memcmp(out, original, sizeof(original)) == 0);
}

void test_namespaces_are_isolated() {
  via::esp32::NVSStorage a;
  assert(a.begin("ns-a"));
  uint8_t data[8];
  memset(data, 0x55, sizeof(data));
  assert(a.write(0, data, sizeof(data)));
  assert(a.commit());

  via::esp32::NVSStorage b;
  assert(b.begin("ns-b"));
  uint8_t out[8];
  assert(b.read(0, out, sizeof(out)));
  for (size_t i = 0; i < sizeof(out); ++i) assert(out[i] == 0xFF);
}

void test_begin_failure() {
  via::esp32::NVSStorage storage;
  assert(!storage.begin(nullptr));
}

int main() {
  test_fresh_state();
  test_roundtrip();
  test_truncated_blob_starts_erased();
  test_missing_blob_starts_erased();
  test_bounds();
  test_erase_is_provisional();
  test_erase_commit_is_durable();
  test_partial_write_is_provisional();
  test_namespaces_are_isolated();
  test_begin_failure();
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
