#pragma once

#include <stddef.h>
#include <stdint.h>

namespace via {

constexpr uint8_t kPacketSize = 32;
constexpr uint8_t kProtocolVersion = 0x0D;
constexpr uint16_t kRawUsagePage = 0xFF60;
constexpr uint16_t kRawUsage = 0x0061;

/* A Raw HID adapter owns the USB endpoint. VIA always exchanges 32-byte
 * packets on a dedicated vendor-defined HID interface. */
class Transport {
 public:
  virtual ~Transport() {}
  virtual bool receive(uint8_t packet[kPacketSize]) = 0;
  virtual bool send(const uint8_t packet[kPacketSize]) = 0;
};

/* Storage implementations may wrap EEPROM, Preferences/NVS, or a reserved
 * flash region. Atomic dual-page flash policy belongs in the adapter. */
class Storage {
 public:
  virtual ~Storage() {}
  virtual size_t capacity() const = 0;
  virtual bool read(size_t offset, uint8_t* output, size_t length) = 0;
  virtual bool write(size_t offset, const uint8_t* input, size_t length) = 0;
  virtual bool commit() = 0;
  virtual bool erase() = 0;
};

class CustomValue {
 public:
  virtual ~CustomValue() {}
  virtual bool set(uint8_t packet[kPacketSize]) = 0;
  virtual bool get(uint8_t packet[kPacketSize]) = 0;
  virtual size_t stateSize() const { return 0; }
  virtual bool saveState(uint8_t*, size_t) const { return true; }
  virtual bool loadState(const uint8_t*, size_t) { return true; }
};

class Callbacks {
 public:
  virtual ~Callbacks() {}
  virtual uint32_t matrixRow(uint8_t row) const { (void)row; return 0; }
  virtual void deviceIndication(uint8_t value) { (void)value; }
  virtual void layoutOptionsChanged(uint32_t value) { (void)value; }
  virtual void changed() {}
  virtual void bootloaderJump() {}
};

struct Config {
  Config(uint8_t rowsValue = 0, uint8_t columnsValue = 0,
         uint8_t layersValue = 0, uint16_t* keymapValue = nullptr,
         const uint16_t* defaultKeymapValue = nullptr,
         uint8_t* macrosValue = nullptr, uint16_t macroBytesValue = 0,
         uint8_t macroCountValue = 0, uint32_t firmwareVersionValue = 0,
         uint32_t autoSaveMsValue = 0, uint32_t defaultLayoutOptionsValue = 0,
         uint8_t encoderCountValue = 0, uint16_t* encoderMapValue = nullptr,
         const uint16_t* defaultEncoderMapValue = nullptr)
      : rows(rowsValue),
        columns(columnsValue),
        layers(layersValue),
        keymap(keymapValue),
        defaultKeymap(defaultKeymapValue),
        macros(macrosValue),
        macroBytes(macroBytesValue),
        macroCount(macroCountValue),
        firmwareVersion(firmwareVersionValue),
        autoSaveMs(autoSaveMsValue),
        defaultLayoutOptions(defaultLayoutOptionsValue),
        encoderCount(encoderCountValue),
        encoderMap(encoderMapValue),
        defaultEncoderMap(defaultEncoderMapValue) {}

  uint8_t rows;
  uint8_t columns;
  uint8_t layers;
  uint16_t* keymap;
  const uint16_t* defaultKeymap;
  uint8_t* macros;
  uint16_t macroBytes;
  uint8_t macroCount;
  uint32_t firmwareVersion;
  uint32_t autoSaveMs;
  uint32_t defaultLayoutOptions;
  uint8_t encoderCount;
  uint16_t* encoderMap;
  const uint16_t* defaultEncoderMap;
};

class Protocol {
 public:
  Protocol(const Config& config, Transport& transport, Storage* storage = nullptr,
           CustomValue* customValue = nullptr, Callbacks* callbacks = nullptr);

  bool begin(uint32_t nowMs = 0);
  void task(uint32_t nowMs);
  bool process(uint8_t packet[kPacketSize], uint32_t nowMs);

  bool load();
  bool save();
  bool factoryReset();
  bool dirty() const { return dirty_; }
  uint16_t keycode(uint8_t layer, uint8_t row, uint8_t column) const;
  bool setKeycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t value);
  uint32_t layoutOptions() const { return layoutOptions_; }
  uint16_t encoderKeycode(uint8_t layer, uint8_t encoder, uint8_t clockwise) const;
  bool setEncoderKeycode(uint8_t layer, uint8_t encoder, uint8_t clockwise,
                         uint16_t value);

 private:
  void resetBuffers();
  void markDirty(uint32_t nowMs);
  size_t keyCount() const;
  size_t keymapBytes() const;
  size_t encoderMapBytes() const;
  size_t stateBytes() const;
  uint32_t stateCrc() const;
  bool readState();
  bool writeState();
  void readDynamicKeymap(uint16_t offset, uint8_t size, uint8_t* output) const;
  void writeDynamicKeymap(uint16_t offset, uint8_t size, const uint8_t* input,
                          uint32_t nowMs);

  Config config_;
  Transport& transport_;
  Storage* storage_;
  CustomValue* customValue_;
  Callbacks* callbacks_;
  bool dirty_;
  uint32_t saveAt_;
  uint32_t layoutOptions_;
};

}  // namespace via
