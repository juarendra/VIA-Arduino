#pragma once

#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include "WroomOtaConfig.h"

class WroomOTA {
 public:
  void begin(void (*releaseKeys)()) {
    ArduinoOTA.onStart([releaseKeys]() {
      releaseKeys();
      Serial.println(F("[OTA] updating"));
    });
    ArduinoOTA.onEnd([]() { Serial.println(F("[OTA] complete")); });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[OTA] error=%u\r\n", static_cast<unsigned>(error));
    });
    Preferences prefs;
    if (prefs.begin("airvia-ota", true)) {
      configured_ = prefs.getBytesLength("config") == sizeof(config_) &&
                    prefs.getBytes("config", &config_, sizeof(config_)) == sizeof(config_) &&
                    wroom_ota::valid(config_);
      prefs.end();
    }
    Serial.println(F("[OTA] provisioning ready"));
    printStatus();
    if (configured_) connect();
  }

  void task() {
    for (unsigned i = 0; i < 32 && Serial.available(); ++i) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\n') {
        if (used_ && line_[used_ - 1] == '\r') --used_;
        line_[used_] = 0;
        if (overflow_) Serial.println(F("[OTA] invalid"));
        else if (strcmp(line_, "OTA_STATUS") == 0) printStatus();
        else if (used_) provision();
        memset(line_, 0, sizeof(line_));
        used_ = 0;
        overflow_ = false;
      } else if (c == 0 || used_ == sizeof(line_) - 1) {
        overflow_ = true;
      } else if (!overflow_) {
        line_[used_++] = c;
      }
    }
    if (!configured_) return;
    if (WiFi.status() != WL_CONNECTED) {
      if (serving_) {
        ArduinoOTA.end();
        serving_ = false;
        Serial.println(F("[OTA] offline"));
      }
      if (static_cast<uint32_t>(millis() - lastConnect_) >= 15000) {
        lastConnect_ = millis();
        WiFi.reconnect();
      }
      return;
    }
    if (!serving_) {
      char hostname[32];
      snprintf(hostname, sizeof(hostname), "airvia-%012llx", ESP.getEfuseMac());
      ArduinoOTA.setHostname(hostname);
      ArduinoOTA.setPasswordHash(config_.otaHash);
      ArduinoOTA.begin();
      serving_ = true;
      printStatus();
    }
    ArduinoOTA.handle();
  }

 private:
  void connect() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(config_.ssid, config_.password);
    lastConnect_ = millis();
  }

  void printStatus() {
    if (WiFi.status() == WL_CONNECTED)
      Serial.printf("[OTA] ip=%s\r\n", WiFi.localIP().toString().c_str());
    else Serial.println(F("[OTA] offline"));
    Serial.printf("[OTA] image=%s\r\n", ESP.getSketchMD5().c_str());
    const esp_partition_t* slot = esp_ota_get_running_partition();
    if (slot) Serial.printf("[OTA] slot=%s\r\n", slot->label);
  }

  void provision() {
    wroom_ota::Config candidate = {};
    if (!wroom_ota::parseSetup(line_, used_, candidate)) {
      Serial.println(F("[OTA] invalid"));
      return;
    }
    Preferences prefs;
    if (!prefs.begin("airvia-ota", false)) {
      Serial.println(F("[OTA] save failed"));
      return;
    }
    const bool saved = prefs.putBytes("config", &candidate, sizeof(candidate)) == sizeof(candidate);
    prefs.end();
    if (!saved) {
      Serial.println(F("[OTA] save failed"));
      return;
    }
    if (serving_) ArduinoOTA.end();
    serving_ = false;
    config_ = candidate;
    memset(&candidate, 0, sizeof(candidate));
    configured_ = true;
    WiFi.disconnect(false, false);
    connect();
    Serial.println(F("[OTA] saved"));
  }

  wroom_ota::Config config_ = {};
  char line_[161] = {};
  size_t used_ = 0;
  uint32_t lastConnect_ = 0;
  bool overflow_ = false;
  bool configured_ = false;
  bool serving_ = false;
};
