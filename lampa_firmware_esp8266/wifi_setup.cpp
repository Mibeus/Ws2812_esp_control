#include "wifi_setup.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>   // rovnaka kniznica (tzapu) funguje pre ESP8266 aj ESP32

static WiFiManager wm;
static char otaPinBuf[16];

static void attachOtaPinParam() {
  strncpy(otaPinBuf, cfg.otaPin, sizeof(otaPinBuf) - 1);
  static WiFiManagerParameter otaPinParam("otapin", "PIN kod pre update firmveru", otaPinBuf, sizeof(otaPinBuf) - 1);
  wm.addParameter(&otaPinParam);
  wm.setSaveParamsCallback([]() {
    // hodnota sa do otaPinBuf zapise automaticky WiFiManagerom pri ulozeni portalu
    strncpy(cfg.otaPin, otaPinBuf, sizeof(cfg.otaPin) - 1);
    configSave();
  });
}

void wifiSetupBegin() {
  attachOtaPinParam();

  // Pri prvom zapnuti (alebo ak sa nepodari pripojit) sa automaticky vytvori
  // hotspot "<MenoZariadenia>-Setup" a zobrazi web s vyberom WiFi siete.
  String apName = String(cfg.deviceName) + "-Setup";
  bool ok = wm.autoConnect(apName.c_str());
  if (!ok) {
    delay(3000);
    ESP.restart();
  }
}

void wifiStartConfigPortal() {
  attachOtaPinParam();
  String apName = String(cfg.deviceName) + "-Setup";
  wm.startConfigPortal(apName.c_str());
}

String wifiGetIp() {
  return WiFi.localIP().toString();
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// Sleduje ci WiFi spojenie zije. Ak vypadne, skusi WiFi.reconnect(); ak sa nepodari
// obnovit do 5 minut, radsej cely ESP restartuje - lepsie kratky vypadok nez
// tiche "zamrznute" zariadenie, ktore treba fyzicky odpojit od napajania.
static bool wifiWasConnected = true;
static uint32_t wifiDownSince = 0;
static uint32_t lastWifiCheck = 0;

void wifiCheckHealth() {
  if (millis() - lastWifiCheck < 5000) return; // kontrola raz za 5s
  lastWifiCheck = millis();

  if (wifiIsConnected()) {
    wifiWasConnected = true;
    wifiDownSince = 0;
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    wifiDownSince = millis();
    Serial.println("[WiFi] spojenie vypadlo, skusam WiFi.reconnect()...");
    WiFi.reconnect();
  } else if (millis() - wifiDownSince > 5UL * 60UL * 1000UL) {
    Serial.println("[WiFi] bez spojenia 5 minut, restartujem zariadenie");
    delay(200);
    ESP.restart();
  }
}
