#pragma once
#include <Arduino.h>

void webServerBegin();
void webServerStop();  // treba zavolat pred spustenim WiFiManager config portalu (rovnaky port 80)
void webServerLoop();
