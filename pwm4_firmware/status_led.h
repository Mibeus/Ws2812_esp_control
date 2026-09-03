#pragma once
#include <Arduino.h>

void statusLedBegin();
void statusLedUpdate(); // volat v loop() - nonblocking, sleduje WiFi a MQTT stav
