// ============================================================
//  Lampa - vlastny firmware pre ESP8266, nahradza Tasmotu
//  HW: ESP8266 (napr. Wemos D1 mini / NodeMCU), WS2812 na D2 (GPIO4), TTP223 na D1 (GPIO5)
//
//  Kniznice (nainstaluj cez Arduino IDE Library Manager):
//   - WiFiManager   (autor: tzapu)
//   - PubSubClient  (autor: knolleary)
//   - Adafruit NeoPixel
//   - ArduinoJson   (autor: bblanchon)
//   - ESP8266HTTPUpdateServer je sucastou ESP8266 Arduino core (netreba instalovat)
//
//  Board settings (Tools menu):
//   - Board: "LOLIN(WEMOS) D1 R2 & mini" (alebo tvoj konkretny ESP8266 modul)
//   - Flash Size: zvol variantu s "OTA" v nazve (napr. "4MB (FS:1MB OTA:~1019KB)"),
//     inak sa novy firmware cez /update nezmesti
//
//  Ovladanie tlacidlom (TTP223 na GPIO5):
//   - 1x klik krátko  -> zapnut/vypnut
//   - 2x klik rychlo  -> dalsi rezim svietenia
//   - podrzanie       -> plynula zmena jasu (smer sa strieda)
//   - 3x klik rychlo  -> spusti WiFi config portal (AP "Lampa-Setup")
// ============================================================

#include "config.h"
#include "led_control.h"
#include "button.h"
#include "wifi_setup.h"
#include "mqtt_client.h"
#include "web_server.h"
#include <ESP8266mDNS.h>

#define PIN_TOUCH 5

// mDNS hostname sa odvodzuje z nazvu zariadenia (napr. "Lampa syn" -> "lampa-syn.local").
// Zmena mena sa v .local adrese prejavi az po restarte (MDNS.begin() bezi len raz v setup()).
static String slugify(const String &name) {
  String out;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (isalnum((unsigned char)c)) {
      out += (char)tolower(c);
    } else if (out.length() > 0 && out[out.length() - 1] != '-') {
      out += '-';
    }
  }
  while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  if (out.length() == 0) out = "lampa";
  return out;
}

static void onSingleClick() {
  cfg.power = !cfg.power;
  configSave();
  ledApplyPower();
  mqttPublishState(cfg.power);
}

static void onDoubleClick() {
  ledNextScheme();
  mqttPublishScheme();
}

static void onTripleClick() {
  // Musime uvolnit port 80, WiFiManager si na nom spusti vlastny config webserver.
  webServerStop();
  wifiStartConfigPortal();   // blokuje, kym sa uzivatel nepripoji / neuplynie timeout
  webServerBegin();
}

static void onHold(int8_t direction) {
  ledAdjustBrightness(direction * 5);
}

static void onHoldEnd() {
  configSave(); // ulozime novy jas az po pusteni, nie kazdych 60ms (setrime flash)
}

// Diagnostika + ochrana pred fragmentaciou pamate pri dlhodobej prevadzke.
// Ak volna pamat kriticky klesne, radsej cisty restart nez neskorsi neurcity pad.
static uint32_t lastHealthCheck = 0;
static void checkSystemHealth() {
  if (millis() - lastHealthCheck < 10000) return; // raz za 10s
  lastHealthCheck = millis();

  uint32_t freeHeap = ESP.getFreeHeap();
  Serial.printf("[Health] volna pamat: %u B, beh: %lu s\n", freeHeap, millis() / 1000);

  if (freeHeap < 8000) {
    Serial.println("[Health] kriticky malo volnej pamate, restartujem zariadenie");
    delay(200);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);

  configBegin();

  ledBegin();
  ledApplyPower();

  buttonBegin(PIN_TOUCH);
  buttonSetCallbacks(onSingleClick, onDoubleClick, onTripleClick, onHold, onHoldEnd);

  wifiSetupBegin();  // prve zapnutie: AP + portal; inak sa pripoji na ulozenu siet

  String mdnsHost = slugify(cfg.deviceName);
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }

  mqttBegin();
  webServerBegin();

  Serial.print("Pripojene. IP adresa: ");
  Serial.println(wifiGetIp());
  Serial.println("Web rozhranie dostupne aj na http://" + mdnsHost + ".local");
}

void loop() {
  buttonUpdate();
  ledUpdate();
  mqttLoop();
  webServerLoop();
  wifiCheckHealth();
  checkSystemHealth();
  MDNS.update();   // na ESP8266 treba mDNS periodicky obsluhovat (na rozdiel od ESP32)
}
