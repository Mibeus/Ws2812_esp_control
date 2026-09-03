#include "status_led.h"
#include "wifi_setup.h"
#include "mqtt_client.h"

#define PIN_STATUS_LED 21

// 3 stavy:
//  - bez WiFi:        vacsinu casu zhasnuta, kratky zablesk cca kazde 2s
//  - WiFi OK, bez MQTT: pravidelne blikanie 1:1 (500ms on / 500ms off)
//  - MQTT pripojene:   trvalo svieti

static uint32_t lastToggle = 0;
static bool ledState = false;

void statusLedBegin() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
}

void statusLedUpdate() {
  bool wifiOk = wifiIsConnected();
  bool mqttOk = mqttIsConnected();

  if (!wifiOk) {
    // kratky zablesk: 50ms svieti kazde 2000ms
    uint32_t phase = millis() % 2000;
    digitalWrite(PIN_STATUS_LED, phase < 50 ? HIGH : LOW);
    return;
  }

  if (mqttOk) {
    digitalWrite(PIN_STATUS_LED, HIGH); // trvalo svieti
    return;
  }

  // WiFi OK, MQTT nie - blikanie 1:1
  if (millis() - lastToggle >= 500) {
    lastToggle = millis();
    ledState = !ledState;
    digitalWrite(PIN_STATUS_LED, ledState ? HIGH : LOW);
  }
}
