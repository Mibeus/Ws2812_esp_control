#include "pins.h"
#include "config.h"
#include "effects_pwm.h"
#include "effects_ws2812.h"
#include "wifi_setup.h"
#include "mqtt_client.h"

// Stavova LED (funkcia FUNC_STATUS_LED) - automaticka signalizacia, ziadne
// nastavenia od pouzivatela. 3 stavy, rovnake ako v predoslom samostatnom module:
//  - bez WiFi:         kratky zablesk cca kazde 2s
//  - WiFi OK, bez MQTT: pravidelne blikanie 1:1
//  - MQTT pripojene:   trvalo svieti
static uint32_t statusLastToggle[NUM_SLOTS] = {0, 0, 0, 0};
static bool statusLedState[NUM_SLOTS] = {false, false, false, false};

static void statusLedUpdateSlot(uint8_t slot) {
  uint8_t pin = cfg.slots[slot].gpio;
  bool wifiOk = wifiIsConnected();
  bool mqttOk = mqttIsConnected();

  if (!wifiOk) {
    uint32_t phase = millis() % 2000;
    digitalWrite(pin, phase < 50 ? HIGH : LOW);
    return;
  }
  if (mqttOk) {
    digitalWrite(pin, HIGH);
    return;
  }
  if (millis() - statusLastToggle[slot] >= 500) {
    statusLastToggle[slot] = millis();
    statusLedState[slot] = !statusLedState[slot];
    digitalWrite(pin, statusLedState[slot] ? HIGH : LOW);
  }
}

void pinsBegin() {
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    SlotConfig &sl = cfg.slots[i];
    switch (sl.function) {
      case FUNC_PWM:
        pwmBeginSlot(i);
        break;
      case FUNC_WS2812:
        ws2812BeginSlot(i);
        break;
      case FUNC_ONOFF:
      case FUNC_STATUS_LED:
        pinMode(sl.gpio, OUTPUT);
        digitalWrite(sl.gpio, LOW);
        break;
      default:
        break; // FUNC_NONE
    }
  }
}

void pinsUpdate() {
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    switch (cfg.slots[i].function) {
      case FUNC_PWM:        pwmUpdateSlot(i);        break;
      case FUNC_WS2812:     ws2812UpdateSlot(i);     break;
      case FUNC_ONOFF:      digitalWrite(cfg.slots[i].gpio, cfg.slots[i].power ? HIGH : LOW); break;
      case FUNC_STATUS_LED: statusLedUpdateSlot(i);  break;
      default: break;
    }
  }
}

void pinsApplyPower(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  switch (cfg.slots[slot].function) {
    case FUNC_PWM:    pwmApplyPower(slot);    break;
    case FUNC_WS2812: ws2812ApplyPower(slot); break;
    case FUNC_ONOFF:  digitalWrite(cfg.slots[slot].gpio, cfg.slots[slot].power ? HIGH : LOW); break;
    default: break;
  }
}

void pinsNextScheme(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  switch (cfg.slots[slot].function) {
    case FUNC_PWM:    pwmNextScheme(slot);    break;
    case FUNC_WS2812: ws2812NextScheme(slot); break;
    default: break; // OnOff a Status LED nemaju rezimy
  }
}
