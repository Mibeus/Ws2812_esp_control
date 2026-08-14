#pragma once
#include <Arduino.h>

void wifiSetupBegin();         // vola sa v setup(): pripoji sa alebo spusti AP portal
void wifiStartConfigPortal();  // vola sa pri 3x kliku v beznej prevadzke
void wifiCheckHealth();        // volat v loop() - sleduje spojenie, skusa reconnect, ako posledna moznost restartuje
String wifiGetIp();
bool wifiIsConnected();
