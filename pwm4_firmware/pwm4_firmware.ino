// ============================================================
//  Svetla - 4 nezavisle PWM kanaly, ESP32-C3 (4MB flash)
//  Piny: kanal 1-4 = GPIO0,1,2,3. Stavova LED = GPIO21.
//
//  Kniznice (Arduino IDE Library Manager):
//   - WiFiManager (tzapu)
//   - PubSubClient (knolleary)
//   - ArduinoJson (bblanchon)
//
//  Board: "ESP32C3 Dev Module", Partition Scheme: bezny 4MB s OTA
//  (napr. "Default 4MB with spiffs" alebo "Minimal SPIFFS")
//
//  Kazdy kanal je uplne nezavisly: vlastny vypinac, jas, rezim.
//  Ziadne fyzicke tlacidlo - ovladanie len cez web/MQTT/Domoticz.
// ============================================================

#include "config.h"
#include "pwm_lights.h"
#include "wifi_setup.h"
#include "mqtt_client.h"
#include "web_server.h"
#include <ESPmDNS.h>
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
  if (out.length() == 0) out = "svetla";
  return out;
}

// Diagnostika + ochrana pred fragmentaciou pamate. Restartuje len ak je
// pamat kriticky malo DLHODOBO (60s v kuse), nie pri jednom ojedinelom vykyve.
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

  configBegin();

  pwmLightsBegin();
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) pwmApplyPower(ch);

  wifiSetupBegin();

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
  pwmLightsUpdate();
  mqttLoop();
  webServerLoop();
  wifiCheckHealth();
  checkSystemHealth();
}
