#include "wifi_setup.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

static WiFiManager wm;
static char otaPinBuf[16];
static uint32_t wifiDownSince = 0;
static const uint32_t WIFI_DOWN_RESTART_MS = 5UL * 60UL * 1000UL;

static void attachOtaPinParam() {
  strncpy(otaPinBuf, cfg.otaPin, sizeof(otaPinBuf) - 1);
  static WiFiManagerParameter otaPinParam("otapin", "PIN kod pre update firmveru", otaPinBuf, sizeof(otaPinBuf) - 1);
  wm.addParameter(&otaPinParam);
  wm.setSaveParamsCallback([]() {
    strncpy(cfg.otaPin, otaPinBuf, sizeof(cfg.otaPin) - 1);
    configSave();
  });
}

void wifiSetupBegin() {
  attachOtaPinParam();
  String apName = String(cfg.deviceName) + "-Setup";
  bool ok = wm.autoConnect(apName.c_str());
  if (!ok) {
    delay(3000);
    ESP.restart();
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // vypnutie WiFi power-save modu (ESP8266 API) - inak prichadzajuce MQTT spravy meskaju 1-3s
}

void wifiStartConfigPortal() {
  attachOtaPinParam();
  String apName = String(cfg.deviceName) + "-Setup";
  wm.startConfigPortal(apName.c_str());
}

void wifiCheckHealth() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiDownSince == 0) wifiDownSince = millis();
    Serial.println("[WiFi] spojenie vypadlo, skusam reconnect");
    WiFi.reconnect();
    if (millis() - wifiDownSince > WIFI_DOWN_RESTART_MS) {
      Serial.println("[WiFi] dlhodobo bez spojenia, restartujem zariadenie");
      delay(200);
      ESP.restart();
    }
  } else {
    wifiDownSince = 0;
  }
}

String wifiGetIp() { return WiFi.localIP().toString(); }
bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }
