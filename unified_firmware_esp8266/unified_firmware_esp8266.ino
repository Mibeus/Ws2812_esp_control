// ============================================================
//  Zjednoteny firmware - ESP32-C3
//  4 nastavitelne piny, funkcia kazdeho sa vybera v /settings:
//    Nevyuzite / Status LED / PWM svetlo / WS2812 pas / On-Off
//
//  Kniznice (Arduino IDE Library Manager):
//   - WiFiManager (tzapu)
//   - PubSubClient (knolleary)
//   - Adafruit NeoPixel
//   - ArduinoJson (bblanchon)
//
//  Board: "ESP32C3 Dev Module", Partition Scheme: hociktora s OTA
// ============================================================

#include "config.h"
#include "pins.h"
#include "wifi_setup.h"
#include "mqtt_client.h"
#include "web_server.h"
#include <ESP8266mDNS.h>
#include <ctype.h>

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
  if (out.length() == 0) out = "zariadenie";
  return out;
}

static uint32_t lastHealthCheck = 0;
static uint8_t lowHeapStreak = 0;
static const uint32_t HEAP_CRITICAL = 6000;
static const uint8_t LOW_HEAP_STREAK_LIMIT = 6;

static void checkSystemHealth() {
  if (millis() - lastHealthCheck < 10000) return;
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
    lowHeapStreak = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Serial start");

  configBegin();
  Serial.println("[BOOT] config nacitany");

  // Ochrana pred boot-loop: ak zariadenie zlyhalo hned po starte 3x po sebe
  // (typicky zla konfiguracia pinu, ktora pada este pred rozbehnutim webservera),
  // na tento pokus preskocime inicializaciu pinov, aby sa dalo dostat do /settings
  // a chybnu konfiguraciu opravit. WiFi/MQTT/webserver bezia normalne aj v SAFE MODE.
  cfg.bootFailCount++;
  configSave();
  bool safeMode = (cfg.bootFailCount >= 3);
  if (safeMode) {
    Serial.println("[BOOT] SAFE MODE - opakovane zlyhanie startu, piny su docasne vypnute");
    pinsSetSafeMode(true);
  } else {
    pinsBegin();
    Serial.println("[BOOT] piny inicializovane");
  }

  wifiSetupBegin();
  Serial.println("[BOOT] WiFi pripojene");

  String mdnsHost = slugify(cfg.deviceName);
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }
  Serial.println("[BOOT] mDNS spustene");

  mqttBegin();
  Serial.println("[BOOT] MQTT inicializovane");

  webServerBegin();
  Serial.println("[BOOT] webserver spusteny");

  Serial.print("Pripojene. IP adresa: ");
  Serial.println(wifiGetIp());
  Serial.println("Web rozhranie dostupne aj na http://" + mdnsHost + ".local");
}

static bool bootStableMarked = false;

void loop() {
  pinsUpdate();
  mqttLoop();
  webServerLoop();
  wifiCheckHealth();
  checkSystemHealth();
  MDNS.update();

  // Po 15s bezproblemoveho behu povazujeme start za uspesny a vynulujeme pocitadlo
  if (!bootStableMarked && millis() > 15000) {
    bootStableMarked = true;
    cfg.bootFailCount = 0;
    configSave();
    Serial.println("[BOOT] beh stabilny 15s, pocitadlo zlyhani vynulovane");
  }
}
