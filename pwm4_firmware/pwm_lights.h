#pragma once
#include <Arduino.h>
#include "config.h"

// Cisla schem - jednoduchsi zoznam nez pri WS2812, ziadna farba (len jas)
enum LightScheme : uint8_t {
  PWM_SCHEME_STATIC = 0,  // pevny jas
  PWM_SCHEME_WAKEUP = 1,  // plynuly nabeh pri zapnuti / zhasnutie pri vypnuti (jednorazovo)
  PWM_SCHEME_PULSE  = 2,  // opakovane plynule stmievanie hore-dole ("dychanie")
  PWM_SCHEME_RANDOM = 3,  // nahodne plynule zmeny jasu
  PWM_SCHEME_CANDLE = 4   // blikajuci plamen
};
#define PWM_SCHEME_COUNT 5

void pwmLightsBegin();
void pwmLightsUpdate();                 // volat v loop() - nonblocking, riadi vsetky 4 kanaly nezavisle
void pwmApplyPower(uint8_t channel);    // ihned zapne/zhasne dany kanal podla cfg.power[channel]
void pwmNextScheme(uint8_t channel);    // posunie dany kanal na dalsi rezim
