#pragma once
#include <Arduino.h>

void wifiSetupBegin();
void wifiStartConfigPortal();
void wifiCheckHealth();
String wifiGetIp();
bool wifiIsConnected();
