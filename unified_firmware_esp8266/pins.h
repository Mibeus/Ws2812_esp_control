#pragma once
#include <Arduino.h>

void pinsBegin();          // inicializuje kazdy slot podla jeho funkcie
void pinsUpdate();         // volat v loop()
void pinsApplyPower(uint8_t slot);
void pinsNextScheme(uint8_t slot);
