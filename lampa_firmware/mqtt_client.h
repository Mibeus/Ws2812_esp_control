#pragma once
#include <Arduino.h>

void mqttBegin();               // (znovu)nacita host/port a nastavi callback
void mqttLoop();                // volat v loop()
void mqttPublishState(bool on); // posle stav lampy do domoticz/in
bool mqttIsConnected();
