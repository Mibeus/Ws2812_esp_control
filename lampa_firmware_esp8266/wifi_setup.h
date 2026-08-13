#pragma once
#include <Arduino.h>

void wifiSetupBegin();         // vola sa v setup(): pripoji sa alebo spusti AP portal
void wifiStartConfigPortal();  // vola sa pri 3x kliku v beznej prevadzke
String wifiGetIp();
bool wifiIsConnected();
