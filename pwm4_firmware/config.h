#pragma once
#include <Arduino.h>

#define NUM_CHANNELS 4

// Vsetky perzistentne nastavenia (ulozene v NVS cez Preferences)
struct Config {
  // Pomenovanie zariadenia (nadpis na strankach, zaklad .local adresy, nazov AP hotspotu)
  char deviceName[32] = "Svetla";

  // MQTT
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPass[32] = "";

  // Domoticz - kazdy kanal ma vlastne IDx (typ "Dimmer")
  int domoticzIdx[NUM_CHANNELS] = {0, 0, 0, 0};

  // PIN kod pre update firmveru (nastavuje sa spolu s WiFi)
  char otaPin[16] = "1234";

  // Kazdy zo 4 kanalov je uplne nezavisly
  bool power[NUM_CHANNELS]      = {true, true, true, true};
  uint8_t brightness[NUM_CHANNELS] = {128, 128, 128, 128}; // 0-255
  uint8_t scheme[NUM_CHANNELS]     = {0, 0, 0, 0};          // 0-4, viz pwm_lights.h
  uint8_t speed[NUM_CHANNELS]      = {8, 8, 8, 8};          // 1-20, rychlost animacii
};

extern Config cfg;

void configBegin();
void configLoad();
void configSave();
