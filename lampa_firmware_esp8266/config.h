#pragma once
#include <Arduino.h>

// Vsetky perzistentne nastavenia lampy (ulozene v NVS cez Preferences / EEPROM)
struct Config {
  // Pomenovanie zariadenia (nadpis na strankach, zaklad pre .local adresu a nazov AP hotspotu)
  char deviceName[32] = "Lampa";

  // MQTT / Domoticz
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPass[32] = "";
  int domoticzIdx = 0;
  int domoticzSchemeIdx = 0;  // Domoticz Selector Switch - vyber rezimu svietenia

  // Pristupovy PIN kod pre update firmveru (nastavuje sa spolu s WiFi)
  char otaPin[16] = "1234";

  // LED / vzhlad
  uint16_t ledCount = 30;     // pocet WS2812 - nastavitelne kvoli inym lampam
  uint8_t brightness = 128;   // 0-255 = jas
  uint8_t saturation = 255;   // 0-255 = sytost farby ("kontrast")
  uint8_t hue = 0;            // 0-255 = odtien farby
  uint8_t scheme = 0;         // aktualny rezim svietenia (Tasmota Scheme cislo)
  bool power = true;
  uint8_t speed = 8;          // rychlost animovanych efektov, 1(pomaly)-20(rychly), tasmota styl
};

extern Config cfg;

void configBegin();   // vola sa raz v setup() - otvori NVS a nacita hodnoty
void configLoad();
void configSave();
