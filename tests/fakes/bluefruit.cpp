#include "bluefruit.h"

void BLEService::begin() {
    FakeBluefruit::serviceUuid = uuid;
}

void BLECharacteristic::begin() {
    if (uuid == 0xFF61) {
        FakeBluefruit::ff61Uuid = uuid;
        FakeBluefruit::ff61FixedLength = fixedLen;
        FakeBluefruit::ff61Cb = writeCb;
    } else if (uuid == 0xFF62) {
        FakeBluefruit::ff62Uuid = uuid;
    }
}

uint16_t BLECharacteristic::write(const void* data, uint16_t len) {
    if (uuid == 0xFF61) {
        memcpy(FakeBluefruit::ff61Written, data, len > 32 ? 32 : len);
        return len;
    } else if (uuid == 0xFF62) {
        memcpy(FakeBluefruit::ff62Value, data, len > 32 ? 32 : len);
        return len;
    }
    return 0;
}

bool BLECharacteristic::notify(const void* data, uint16_t len) {
    (void)data;
    (void)len;
    if (uuid == 0xFF61) {
        if (FakeBluefruit::ff61NotifySuccess) {
            FakeBluefruit::ff61NotifyCount++;
            return true;
        }
        return false;
    }
    return false;
}

bool BLECharacteristic::notifyEnabled() {
    if (uuid == 0xFF61) return FakeBluefruit::ff61Notifying;
    return false;
}
