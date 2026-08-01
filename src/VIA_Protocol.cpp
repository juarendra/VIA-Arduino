#include "VIA_Protocol.h"

#include <string.h>

namespace via {
namespace {

constexpr uint32_t kStateMagic = 0x56494141UL;  // "VIAA"
constexpr uint16_t kStateVersion = 2;

struct __attribute__((packed)) StateHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t payloadSize;
  uint32_t crc;
};

uint32_t crc32Update(uint32_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1U)));
  }
  return crc;
}

}  // namespace

Protocol::Protocol(const Config& config, Transport& transport, Storage* storage,
                   CustomValue* customValue, Callbacks* callbacks)
    : config_(config),
      transport_(transport),
      storage_(storage),
      customValue_(customValue),
      callbacks_(callbacks),
      dirty_(false),
      saveAt_(0),
      layoutOptions_(config.defaultLayoutOptions) {}

bool Protocol::begin(uint32_t nowMs) {
  size_t bytes;
  size_t customBytes;
  if (config_.rows == 0 || config_.columns == 0 || config_.layers == 0 ||
      config_.keymap == nullptr || config_.defaultKeymap == nullptr ||
      (config_.macroBytes != 0 && config_.macros == nullptr) ||
      (config_.encoderCount != 0 &&
       (config_.encoderMap == nullptr || config_.defaultEncoderMap == nullptr)) ||
      !stateBytes(bytes, customBytes) ||
      (storage_ && (config_.loadBuffer == nullptr || config_.loadBufferBytes < bytes))) {
    return false;
  }
  if (!load()) resetBuffers();
  dirty_ = false;
  saveAt_ = nowMs + config_.autoSaveMs;
  return true;
}

void Protocol::task(uint32_t nowMs) {
  uint8_t packet[kPacketSize];
  if (transport_.receive(packet)) {
    process(packet, nowMs);
    transport_.send(packet);
  }
  if (dirty_ && static_cast<int32_t>(nowMs - saveAt_) >= 0) save();
}

bool Protocol::process(uint8_t packet[kPacketSize], uint32_t nowMs) {
  switch (packet[0]) {
    case 0x01:  // get protocol version
      packet[1] = 0;
      packet[2] = kProtocolVersion;
      break;
    case 0x02:  // get keyboard value
      switch (packet[1]) {
        case 0x01:  // uptime
          packet[2] = static_cast<uint8_t>(nowMs >> 24);
          packet[3] = static_cast<uint8_t>(nowMs >> 16);
          packet[4] = static_cast<uint8_t>(nowMs >> 8);
          packet[5] = static_cast<uint8_t>(nowMs);
          break;
        case 0x02:  // layout options
          packet[2] = static_cast<uint8_t>(layoutOptions_ >> 24);
          packet[3] = static_cast<uint8_t>(layoutOptions_ >> 16);
          packet[4] = static_cast<uint8_t>(layoutOptions_ >> 8);
          packet[5] = static_cast<uint8_t>(layoutOptions_);
          break;
        case 0x03: { // switch matrix state
          const uint8_t bytesPerRow = (config_.columns + 7) / 8;
          if (bytesPerRow == 0 || bytesPerRow > 4) {
            packet[0] = 0xFF;
            break;
          }
          const uint8_t maxRows = 28 / bytesPerRow;
          const uint8_t startRow = packet[2];
          uint8_t outIndex = 3;
          for (uint8_t i = 0; i < maxRows; ++i) {
            const uint16_t row = static_cast<uint16_t>(startRow) + i;
            const uint32_t rowData = (callbacks_ && row < config_.rows)
                                         ? callbacks_->matrixRow(static_cast<uint8_t>(row))
                                         : 0;
            for (uint8_t b = 0; b < bytesPerRow; ++b) {
              packet[outIndex++] = static_cast<uint8_t>(
                  rowData >> (8 * (bytesPerRow - b - 1)));
            }
          }
          while (outIndex < kPacketSize) packet[outIndex++] = 0;
          break;
        }
        case 0x04:  // firmware version
          packet[2] = static_cast<uint8_t>(config_.firmwareVersion >> 24);
          packet[3] = static_cast<uint8_t>(config_.firmwareVersion >> 16);
          packet[4] = static_cast<uint8_t>(config_.firmwareVersion >> 8);
          packet[5] = static_cast<uint8_t>(config_.firmwareVersion);
          break;
        case 0x06:  // QMK keycodes version 0.0.8
          packet[2] = packet[3] = packet[4] = 0;
          packet[5] = 8;
          break;
        default:
          packet[0] = 0xFF;
      }
      break;
    case 0x03:  // set keyboard value
      if (packet[1] == 0x05) {
        if (callbacks_) callbacks_->deviceIndication(packet[2]);
      } else if (packet[1] == 0x02) {
        layoutOptions_ = (static_cast<uint32_t>(packet[2]) << 24) |
                         (static_cast<uint32_t>(packet[3]) << 16) |
                         (static_cast<uint32_t>(packet[4]) << 8) | packet[5];
        markDirty(nowMs);
        if (callbacks_) callbacks_->layoutOptionsChanged(layoutOptions_);
      } else {
        packet[0] = 0xFF;
      }
      break;
    case 0x04: {  // dynamic keymap get keycode
      const uint16_t code = keycode(packet[1], packet[2], packet[3]);
      packet[4] = static_cast<uint8_t>(code >> 8);
      packet[5] = static_cast<uint8_t>(code);
      break;
    }
    case 0x05:  // dynamic keymap set keycode
      if (!setKeycode(packet[1], packet[2], packet[3],
                      static_cast<uint16_t>(packet[4] << 8 | packet[5]))) {
        packet[0] = 0xFF;
      } else {
        markDirty(nowMs);
      }
      break;
    case 0x06:  // dynamic keymap reset
      memcpy(config_.keymap, config_.defaultKeymap, keymapBytes());
      if (encoderMapBytes()) {
        memcpy(config_.encoderMap, config_.defaultEncoderMap, encoderMapBytes());
      }
      markDirty(nowMs);
      break;
    case 0x07:  // set custom value
      if (!customValue_ || !customValue_->set(packet)) packet[0] = 0xFF;
      else markDirty(nowMs);
      break;
    case 0x08:  // get custom value
      if (!customValue_ || !customValue_->get(packet)) packet[0] = 0xFF;
      break;
    case 0x09:  // EEPROM reset / commit
      if (!customValue_ || !customValue_->save(packet) || !save()) packet[0] = 0xFF;
      break;
    case 0x0A:  // reset EEPROM
      if (!factoryReset()) packet[0] = 0xFF;
      break;
    case 0x0B:  // bootloader jump
      if (callbacks_) callbacks_->bootloaderJump();
      break;
    case 0x0C:  // get macro count
      packet[1] = config_.macroCount;
      break;
    case 0x0D:  // get macro buffer size
      packet[1] = static_cast<uint8_t>(config_.macroBytes >> 8);
      packet[2] = static_cast<uint8_t>(config_.macroBytes);
      break;
    case 0x0E: {  // get macro buffer
      const uint16_t offset = static_cast<uint16_t>(packet[1] << 8 | packet[2]);
      const uint8_t size = packet[3] > 28 ? 28 : packet[3];
      for (uint8_t i = 0; i < size; ++i) {
        const uint32_t index = static_cast<uint32_t>(offset) + i;
        packet[4 + i] = index < config_.macroBytes ? config_.macros[index] : 0;
      }
      break;
    }
    case 0x0F: {  // set macro buffer
      const uint16_t offset = static_cast<uint16_t>(packet[1] << 8 | packet[2]);
      const uint8_t size = packet[3] > 28 ? 28 : packet[3];
      bool wrote = false;
      for (uint8_t i = 0; i < size; ++i) {
        const uint32_t index = static_cast<uint32_t>(offset) + i;
        if (index < config_.macroBytes) {
          config_.macros[index] = packet[4 + i];
          wrote = true;
        }
      }
      if (wrote) markDirty(nowMs);
      break;
    }
    case 0x10:  // reset macros
      if (config_.macroBytes) memset(config_.macros, 0, config_.macroBytes);
      markDirty(nowMs);
      break;
    case 0x11:  // get layer count
      packet[1] = config_.layers;
      break;
    case 0x12: {  // get dynamic keymap buffer
      const uint16_t offset = static_cast<uint16_t>(packet[1] << 8 | packet[2]);
      readDynamicKeymap(offset, packet[3] > 28 ? 28 : packet[3], &packet[4]);
      break;
    }
    case 0x13: {  // set dynamic keymap buffer
      const uint16_t offset = static_cast<uint16_t>(packet[1] << 8 | packet[2]);
      writeDynamicKeymap(offset, packet[3] > 28 ? 28 : packet[3], &packet[4], nowMs);
      break;
    }
    case 0x14: {  // dynamic keymap get encoder
      if (packet[1] >= config_.layers || packet[2] >= config_.encoderCount ||
          packet[3] > 1) {
        packet[0] = 0xFF;
        break;
      }
      const uint16_t code = encoderKeycode(packet[1], packet[2], packet[3]);
      packet[4] = static_cast<uint8_t>(code >> 8);
      packet[5] = static_cast<uint8_t>(code);
      break;
    }
    case 0x15:  // dynamic keymap set encoder
      if (!setEncoderKeycode(packet[1], packet[2], packet[3],
                             static_cast<uint16_t>(
                                 static_cast<uint16_t>(packet[4]) << 8 | packet[5]))) {
        packet[0] = 0xFF;
      } else {
        markDirty(nowMs);
      }
      break;
    default:
      packet[0] = 0xFF;
  }
  return packet[0] != 0xFF;
}

uint16_t Protocol::keycode(uint8_t layer, uint8_t row, uint8_t column) const {
  if (layer >= config_.layers || row >= config_.rows || column >= config_.columns) return 0;
  return config_.keymap[(static_cast<size_t>(layer) * config_.rows + row) *
                         config_.columns + column];
}

bool Protocol::setKeycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t value) {
  if (layer >= config_.layers || row >= config_.rows || column >= config_.columns) return false;
  config_.keymap[(static_cast<size_t>(layer) * config_.rows + row) *
                 config_.columns + column] = value;
  return true;
}

uint16_t Protocol::encoderKeycode(uint8_t layer, uint8_t encoder,
                                  uint8_t clockwise) const {
  if (layer >= config_.layers || encoder >= config_.encoderCount || clockwise > 1) return 0;
  return config_.encoderMap[(static_cast<size_t>(layer) * config_.encoderCount + encoder) * 2 +
                            clockwise];
}

bool Protocol::setEncoderKeycode(uint8_t layer, uint8_t encoder,
                                 uint8_t clockwise, uint16_t value) {
  if (layer >= config_.layers || encoder >= config_.encoderCount || clockwise > 1) return false;
  config_.encoderMap[(static_cast<size_t>(layer) * config_.encoderCount + encoder) * 2 +
                     clockwise] = value;
  return true;
}

void Protocol::resetBuffers() {
  memcpy(config_.keymap, config_.defaultKeymap, keymapBytes());
  if (encoderMapBytes()) {
    memcpy(config_.encoderMap, config_.defaultEncoderMap, encoderMapBytes());
  }
  if (config_.macroBytes) memset(config_.macros, 0, config_.macroBytes);
  layoutOptions_ = config_.defaultLayoutOptions;
}

void Protocol::markDirty(uint32_t nowMs) {
  dirty_ = true;
  saveAt_ = nowMs + config_.autoSaveMs;
  if (callbacks_) callbacks_->changed();
}

size_t Protocol::keyCount() const {
  return static_cast<size_t>(static_cast<uint32_t>(config_.rows) * config_.columns *
                             config_.layers);
}

size_t Protocol::keymapBytes() const { return keyCount() * sizeof(uint16_t); }

size_t Protocol::encoderMapBytes() const {
  return static_cast<size_t>(static_cast<uint32_t>(config_.layers) *
                             config_.encoderCount * 2U * sizeof(uint16_t));
}

bool Protocol::stateBytes(size_t& bytes, size_t& customBytes) const {
  customBytes = customValue_ ? customValue_->stateSize() : 0;
  if (customBytes > kMaxCustomStateSize) return false;

  const uint32_t keymap = static_cast<uint32_t>(config_.rows) * config_.columns *
                          config_.layers * sizeof(uint16_t);
  const uint32_t encoders = static_cast<uint32_t>(config_.layers) *
                            config_.encoderCount * 2U * sizeof(uint16_t);
  const uint32_t total = keymap + encoders + config_.macroBytes +
                         sizeof(layoutOptions_) + static_cast<uint32_t>(customBytes);
  const uint32_t record = total + sizeof(StateHeader);
  if (record > UINT16_MAX) return false;
  bytes = static_cast<size_t>(total);
  return true;
}

uint32_t Protocol::stateCrc(const uint8_t* customState, size_t customSize) const {
  uint32_t crc = 0xFFFFFFFFUL;
  const uint8_t* keymap = reinterpret_cast<const uint8_t*>(config_.keymap);
  for (size_t i = 0; i < keymapBytes(); ++i) crc = crc32Update(crc, keymap[i]);
  const uint8_t* encoderMap = reinterpret_cast<const uint8_t*>(config_.encoderMap);
  for (size_t i = 0; i < encoderMapBytes(); ++i) crc = crc32Update(crc, encoderMap[i]);
  for (uint16_t i = 0; i < config_.macroBytes; ++i) crc = crc32Update(crc, config_.macros[i]);
  const uint8_t* layoutOptions = reinterpret_cast<const uint8_t*>(&layoutOptions_);
  for (size_t i = 0; i < sizeof(layoutOptions_); ++i) {
    crc = crc32Update(crc, layoutOptions[i]);
  }
  for (size_t i = 0; i < customSize; ++i) crc = crc32Update(crc, customState[i]);
  return ~crc;
}

bool Protocol::load() {
  size_t bytes;
  size_t customSize;
  if (!storage_ || !stateBytes(bytes, customSize)) return false;
  if (config_.loadBuffer == nullptr || config_.loadBufferBytes < bytes) return false;
  if (storage_->capacity() < sizeof(StateHeader) + bytes) return false;
  StateHeader header;
  if (!storage_->read(0, reinterpret_cast<uint8_t*>(&header), sizeof(header)) ||
      header.magic != kStateMagic || header.version != kStateVersion ||
      header.payloadSize != bytes) return false;

  uint8_t state[kMaxCustomStateSize];
  size_t offset = sizeof(header);
  size_t remaining = bytes;
  uint32_t crc = 0xFFFFFFFFUL;
  while (remaining) {
    const size_t chunk = remaining < sizeof(state) ? remaining : sizeof(state);
    if (!storage_->read(offset, state, chunk)) return false;
    for (size_t i = 0; i < chunk; ++i) crc = crc32Update(crc, state[i]);
    offset += chunk;
    remaining -= chunk;
  }
  if (~crc != header.crc) return false;

  offset = sizeof(header);
  size_t staged = 0;
  if (!storage_->read(offset, config_.loadBuffer + staged, keymapBytes())) return false;
  offset += keymapBytes();
  staged += keymapBytes();
  if (encoderMapBytes() &&
      !storage_->read(offset, config_.loadBuffer + staged,
                      encoderMapBytes())) return false;
  offset += encoderMapBytes();
  staged += encoderMapBytes();
  if (config_.macroBytes &&
      !storage_->read(offset, config_.loadBuffer + staged, config_.macroBytes)) return false;
  offset += config_.macroBytes;
  staged += config_.macroBytes;
  if (!storage_->read(offset, config_.loadBuffer + staged, sizeof(layoutOptions_))) return false;
  offset += sizeof(layoutOptions_);
  staged += sizeof(layoutOptions_);
  if (customSize && !storage_->read(offset, config_.loadBuffer + staged, customSize)) return false;
  if (customSize && !customValue_->loadState(config_.loadBuffer + staged, customSize)) {
    return false;
  }
  staged = 0;
  memcpy(config_.keymap, config_.loadBuffer + staged, keymapBytes());
  staged += keymapBytes();
  if (encoderMapBytes()) {
    memcpy(config_.encoderMap, config_.loadBuffer + staged, encoderMapBytes());
  }
  staged += encoderMapBytes();
  if (config_.macroBytes) {
    memcpy(config_.macros, config_.loadBuffer + staged, config_.macroBytes);
  }
  staged += config_.macroBytes;
  memcpy(&layoutOptions_, config_.loadBuffer + staged, sizeof(layoutOptions_));
  return true;
}

bool Protocol::save() {
  if (!storage_) return false;
  return writeState();
}

bool Protocol::writeState() {
  size_t bytes;
  size_t customSize;
  if (!stateBytes(bytes, customSize)) return false;
  if (storage_->capacity() < sizeof(StateHeader) + bytes) return false;
  uint8_t customState[kMaxCustomStateSize];
  if (customSize && !customValue_->saveState(customState, customSize)) return false;
  StateHeader header = {kStateMagic, kStateVersion, static_cast<uint16_t>(bytes),
                        stateCrc(customState, customSize)};
  const uint32_t crc = header.crc;
  header.crc = 0;
  if (!storage_->write(0, reinterpret_cast<const uint8_t*>(&header), sizeof(header))) return false;
  size_t offset = sizeof(header);
  if (!storage_->write(offset, reinterpret_cast<const uint8_t*>(config_.keymap), keymapBytes())) return false;
  offset += keymapBytes();
  if (encoderMapBytes() &&
      !storage_->write(offset, reinterpret_cast<const uint8_t*>(config_.encoderMap),
                       encoderMapBytes())) return false;
  offset += encoderMapBytes();
  if (config_.macroBytes && !storage_->write(offset, config_.macros, config_.macroBytes)) return false;
  offset += config_.macroBytes;
  if (!storage_->write(offset, reinterpret_cast<const uint8_t*>(&layoutOptions_),
                       sizeof(layoutOptions_))) return false;
  offset += sizeof(layoutOptions_);
  if (customSize && !storage_->write(offset, customState, customSize)) return false;
  header.crc = crc;
  if (!storage_->write(0, reinterpret_cast<const uint8_t*>(&header), sizeof(header)) ||
      !storage_->commit()) return false;
  dirty_ = false;
  return true;
}

bool Protocol::factoryReset() {
  size_t bytes;
  size_t customSize;
  if (!stateBytes(bytes, customSize)) return false;
  resetBuffers();
  if (customSize) {
    uint8_t state[kMaxCustomStateSize] = {};
    if (!customValue_->loadState(state, customSize)) return false;
  }
  if (!storage_) return false;
  if (!storage_->erase()) return false;
  dirty_ = true;
  return save();
}

void Protocol::readDynamicKeymap(uint16_t offset, uint8_t size, uint8_t* output) const {
  const size_t bytes = keymapBytes();
  for (uint8_t i = 0; i < size; ++i) {
    const uint32_t index = static_cast<uint32_t>(offset) + i;
    if (index >= bytes) {
      output[i] = 0;
      continue;
    }
    const uint16_t keycode = config_.keymap[index / 2];
    output[i] = (index & 1U) ? static_cast<uint8_t>(keycode)
                              : static_cast<uint8_t>(keycode >> 8);
  }
}

void Protocol::writeDynamicKeymap(uint16_t offset, uint8_t size, const uint8_t* input,
                                  uint32_t nowMs) {
  const size_t bytes = keymapBytes();
  bool wrote = false;
  for (uint8_t i = 0; i < size; ++i) {
    const uint32_t index = static_cast<uint32_t>(offset) + i;
    if (index >= bytes) continue;
    uint16_t& keycode = config_.keymap[index / 2];
    keycode = (index & 1U) ? static_cast<uint16_t>((keycode & 0xFF00U) | input[i])
                           : static_cast<uint16_t>((keycode & 0x00FFU) | (input[i] << 8));
    wrote = true;
  }
  if (wrote) markDirty(nowMs);
}

}  // namespace via
