#include "wifi_setup.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>

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
