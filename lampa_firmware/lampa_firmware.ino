// ============================================================
//  Lampa - vlastny firmware pre ESP32-C3, nahradza Tasmotu
//  HW: ESP32-C3 Super Mini, WS2812 na GPIO4, TTP223 na GPIO5
//
//  Kniznice (nainstaluj cez Arduino IDE Library Manager):
//   - WiFiManager   (autor: tzapu)
//   - PubSubClient  (autor: knolleary)
//   - Adafruit NeoPixel
//   - ArduinoJson   (autor: bblanchon)
//
//  Board settings (Tools menu):
//   - Board: "ESP32C3 Dev Module"
//   - Partition Scheme: "Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)"
//     (potrebne kvoli 4MB flash + 2x OTA app partícia)
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
#include <ESPmDNS.h>

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
// Restartujeme len ak je pamat kriticky malo DLHODOBO (nie pri jednom ojedinelom
// vykyve, napr. pocas startu kym sa naraz inicializuje WiFiManager/MQTT/webserver).
static uint32_t lastHealthCheck = 0;
static uint8_t lowHeapStreak = 0;
static const uint32_t HEAP_CRITICAL = 6000;      // prah pre zapocitanie "streaku"
static const uint8_t LOW_HEAP_STREAK_LIMIT = 6;  // 6x po sebe (60s v kuse) az potom restart

static void checkSystemHealth() {
  if (millis() - lastHealthCheck < 10000) return; // raz za 10s
  lastHealthCheck = millis();

  uint32_t freeHeap = ESP.getFreeHeap();
  Serial.printf("[Health] volna pamat: %u B, beh: %lu s\n", freeHeap, millis() / 1000);

  if (freeHeap < HEAP_CRITICAL) {
    lowHeapStreak++;
    Serial.printf("[Health] pod kritickou hranicou (%u/%u kontrol po sebe)\n", lowHeapStreak, LOW_HEAP_STREAK_LIMIT);
    if (lowHeapStreak >= LOW_HEAP_STREAK_LIMIT) {
      Serial.println("[Health] dlhodobo kriticky malo pamate, restartujem zariadenie");
      delay(200);
      ESP.restart();
    }
  } else {
    lowHeapStreak = 0; // ojedinely pokles bez pokracovania nie je dovod na restart
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
}
