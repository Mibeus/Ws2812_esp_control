#pragma once
#include <Arduino.h>

void mqttBegin();               // (znovu)nacita host/port a nastavi callback
void mqttLoop();                // volat v loop()
void mqttPublishState(bool on); // posle stav lampy do domoticz/in
void mqttPublishColor();        // posle aktualnu farbu + jas (Color Switch idx)
void mqttPublishScheme();       // posle aktualny rezim (Selector Switch idx)
bool mqttIsConnected();
