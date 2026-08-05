#include <Arduino.h>
#include <InternalFileSystem.h>

struct RecordHeader {
  uint32_t magic;
  uint32_t generation;
  uint32_t length;
  uint32_t crc32;
};

static bool readHeader(const char* path, RecordHeader& header) {
  Adafruit_LittleFS_Namespace::File file =
      InternalFS.open(path, Adafruit_LittleFS_Namespace::FILE_O_READ);
  if (!file) return false;
  const bool ok = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) ==
                  sizeof(header);
  file.close();
  return ok && header.magic == 0x56494146 && header.length == 4096;
}

static bool newer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  if (!InternalFS.begin()) {
    Serial.println("FAIL: mount");
    return;
  }

  RecordHeader a = {}, b = {};
  if (!readHeader("/via_a.dat", a) || !readHeader("/via_b.dat", b)) {
    Serial.println("FAIL: two committed slots required");
    return;
  }

  const char* newest = newer(a.generation, b.generation)
                           ? "/via_a.dat" : "/via_b.dat";
  Adafruit_LittleFS_Namespace::File file =
      InternalFS.open(newest, Adafruit_LittleFS_Namespace::FILE_O_WRITE);
  if (!file) {
    Serial.println("FAIL: open newest slot");
    return;
  }
  RecordHeader invalid = {};
  const bool ok = file.write(reinterpret_cast<const uint8_t*>(&invalid),
                             sizeof(invalid)) == sizeof(invalid);
  file.close();
  Serial.println(ok ? "PASS: newest slot corrupted" : "FAIL: write");
}

void loop() {}
