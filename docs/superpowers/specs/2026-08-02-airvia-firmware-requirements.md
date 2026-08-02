# AirVIA Firmware Requirements — BLE VIA Transport

## Tujuan

Bikin BLE GATT transport adapter di sisi firmware (ESP32-S3) agar AirVIA web app bisa konfigurasi keyboard via BLE, pakai format packet VIA v13 yang sama dengan USB Raw HID.

## Target Hardware

- ESP32-S3 (ada USB device peripheral + BLE)
- NimBLE stack (via NimBLE-Arduino)
- Bisa coexist dengan TinyUSB — USB buat charging/debug, BLE buat VIA config

## GATT Service

```
VIA Service UUID:   0000FF60-0000-1000-8000-00805F9B34FB

Characteristic FF61 (VIA Data):
  UUID:   0000FF61-0000-1000-8000-00805F9B34FB
  Props:  Read | Write | Notify
  Size:   32 bytes fixed
  Logic:  Write = VIA command dari app
          Notify = VIA response ke app (setelah write diproses)

Characteristic FF62 (VIA Info):
  UUID:   0000FF62-0000-1000-8000-00805F9B34FB
  Props:  Read
  Value:  4 byte firmware version (uint32 BE) + device name string (UTF-8, up to 28 byte)
          Total max 32 byte
```

## Packet Protocol

SAMA PERSIS dengan VIA v13 di `VIA_Protocol.cpp`. Tidak ada perubahan format.

- Packet: 32 byte fixed
- Command ID di byte 0
- Response: 32 byte yang sama, isi di-modify
- Error: byte 0 = 0xFF

### Flow

```
1. App connect BLE → discover FF60 service
2. App subscribe notify FF61
3. App read FF62 → dapat firmware version + device name
4. App write 32B ke FF61 → firmware process → firmware notify 32B response ke FF61
5. Repeat step 4 untuk semua command (keymap, encoder, macro, lighting, dll)
```

### Minimal Command yang HARUS jalan

| Command | ID | Fungsi |
|---|---|---|
| Get protocol version | 0x01 | Return 0x000D (v13) |
| Get layer count | 0x11 | Return jumlah layer |
| Get keymap buffer | 0x12 | Bulk read keymap bytes |
| Set keycode | 0x05 | Set satu keycode |
| Set keymap buffer | 0x13 | Bulk write keymap |
| Reset keymap | 0x06 | Restore default |
| Save | 0x09 | Commit ke storage |

Sisanya (macro, encoder, lighting, layout options, factory reset, bootloader) bisa nyusul — protocol sama, implementasi per command mengikuti `VIA_Protocol.cpp`.

## Integrasi dengan VIA-Arduino

`VIA_Protocol` class di `VIA_Protocol.h` sudah agnostik transport:

```cpp
class Transport {
 public:
  virtual bool receive(uint8_t packet[kPacketSize]) = 0;
  virtual bool send(const uint8_t packet[kPacketSize]) = 0;
  virtual bool sendComplete() { return true; }
};
```

Yang perlu dibikin: `VIA_ESP32S3_BLE_ViaTransport` implement `Transport` interface.

### Arsitektur

```
[AirVIA web app]
     | BLE GATT (FF60/FF61/FF62)
     v
[VIA_ESP32S3_BLE_ViaTransport : via::Transport]
     | receive() → ambil dari FF61 write buffer
     | send()    → notify FF61
     v
[via::Protocol]  ← persis sama, tidak disentuh
     |
     v
[via::Matrix + via::Keyboard + etc]
```

Sketch ESP32-S3: USB buat TinyUSB Raw HID (kalau colok kabel masih bisa VIA via USB), BLE buat wireless config. Dua transport jalan bareng — Protocol bisa terima packet dari dua sumber berbeda (pakai `process()` bukan `task()` kalau perlu multi-transport).

### Implementasi Teknis

```cpp
class BLEViaTransport : public via::Transport {
 public:
  bool begin();  // init NimBLE GATT server
  bool receive(uint8_t packet[via::kPacketSize]) override {
    // return true kalau ada packet dari write callback
    // copy dari rx_buffer_, clear rx_ready_
  }
  bool send(const uint8_t packet[via::kPacketSize]) override {
    // notify FF61 dengan packet
    // return true kalau notify queue sukses
  }
  bool sendComplete() override {
    // return true kalau notify sebelumnya sudah terkirim
  }

 private:
  NimBLEServer* server_;
  NimBLECharacteristic* dataChar_;  // FF61
  NimBLECharacteristic* infoChar_;  // FF62
  uint8_t rx_buffer_[32];
  volatile bool rx_ready_;
  volatile bool tx_complete_;
};
```

### Callback Wiring

```cpp
class WriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    // copy c->getValue() ke rx_buffer_
    // set rx_ready_ = true
  }
};

class NotifyCallback : public NimBLECharacteristicCallbacks {
  void onNotify(NimBLECharacteristic* c) override {
    // set tx_complete_ = true
  }
};
```

## Non-Requirements

- TIDAK perlu BLE HID keyboard (typing) — itu sudah ada di `VIA_ESP32S3_BLE.h`
- TIDAK perlu security pairing khusus — BLE Just Works cukup
- TIDAK perlu handle disconnect reconnect otomatis — app side yang handle
- TIDAK perlu GATT server info/model number characteristic — cuma FF61 dan FF62

## Testing

1. Flash ke ESP32-S3
2. Buka AirVIA di Chrome → Connect → harus muncul di device list
3. Load V3 definition JSON → keymap grid muncul
4. Baca keymap dari device → grid terisi
5. Edit keycode → kirim ke device → baca ulang → keycode berubah
6. Save (0x09) → kirim → device commit ke NVS
7. Disconnect → reconnect → keymap masih sama (persistent)

## Prioritas

1. FF61 write + notify + 0x01/0x11/0x12/0x05 command (minimal viable)
2. FF62 info characteristic
3. 0x13/0x06/0x09 (save/reset)
4. Commands sisanya (encoder, macro, lighting, dll)
