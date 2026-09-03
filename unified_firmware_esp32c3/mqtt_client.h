#pragma once
#include <Arduino.h>

void mqttBegin();
void mqttLoop();
void mqttPublishSlot(uint8_t slot);       // posle stav (+farba/jas) daneho slotu do domoticz/in
void mqttPublishSlotScheme(uint8_t slot); // posle aktualny rezim (Selector Switch idx), ak je nastaveny
bool mqttIsConnected();
