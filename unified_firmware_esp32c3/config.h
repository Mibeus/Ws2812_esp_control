#pragma once
#include <Arduino.h>

#define NUM_SLOTS 4

enum PinFunction : uint8_t {
  FUNC_NONE       = 0,  // pin nevyuzity
  FUNC_STATUS_LED = 1,  // automaticka signalizacia WiFi/MQTT stavu
  FUNC_PWM        = 2,  // jedno nezavisle stmievatelne svetlo (bez farby)
  FUNC_WS2812     = 3,  // adresovatelny LED pas (farba + efekty)
  FUNC_ONOFF      = 4   // jednoduchy vystup on/off (napr. rele)
};
#define FUNC_COUNT 5

// Nastavenia jedneho slotu/pinu. Nie vsetky polia davaju zmysel pre kazdu
// funkciu (napr. hue/saturation len pre WS2812) - nevyuzite jednoducho
// ostanu nepouzite, nestoji to takmer ziadnu pamat navyse.
struct SlotConfig {
  uint8_t gpio = 0;
  uint8_t function = FUNC_NONE;

  bool power = true;
  uint8_t brightness = 128;     // 0-255, PWM aj WS2812
  uint8_t hue = 0;               // 0-255, len WS2812
  uint8_t saturation = 255;      // 0-255, len WS2812
  uint8_t scheme = 0;            // cislo rezimu, vyznam zavisi od funkcie
  uint8_t speed = 8;              // 1-20, rychlost animacii
  uint16_t ledCount = 30;        // len WS2812

  int domoticzIdx = 0;           // hlavne zariadenie (Switch/Dimmer/Color Switch)
  int domoticzSchemeIdx = 0;     // volitelny Selector Switch pre vyber rezimu (PWM/WS2812)
};

struct Config {
  char deviceName[32] = "Zariadenie";

  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPass[32] = "";

  char otaPin[16] = "1234";

  SlotConfig slots[NUM_SLOTS];
};

extern Config cfg;

void configBegin();
void configLoad();
void configSave();
