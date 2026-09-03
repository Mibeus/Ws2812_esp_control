#pragma once
#include <Arduino.h>

void mqttBegin();
void mqttLoop();
void mqttPublishChannel(uint8_t channel); // posle stav+jas daneho kanala do domoticz/in
bool mqttIsConnected();
