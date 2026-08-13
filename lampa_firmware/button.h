#pragma once
#include <Arduino.h>

// Callbacky, ktore hlavny .ino nastavi cez buttonSetCallbacks()
typedef void (*ButtonSingleCb)();
typedef void (*ButtonDoubleCb)();
typedef void (*ButtonTripleCb)();
typedef void (*ButtonHoldCb)(int8_t direction);  // vola sa opakovane pocas drzania
typedef void (*ButtonHoldEndCb)();               // vola sa raz, ked sa drzanie skonci

void buttonBegin(uint8_t pin);
void buttonUpdate();  // volat v loop() - nonblocking
void buttonSetCallbacks(ButtonSingleCb, ButtonDoubleCb, ButtonTripleCb,
                         ButtonHoldCb, ButtonHoldEndCb);
