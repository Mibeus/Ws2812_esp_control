#pragma once
#include <Arduino.h>

// Cisla schem su zvolene tak, aby sa zhodovali s Tasmota "Scheme" prikazom
enum LightScheme : uint8_t {
  SCHEME_SINGLE      = 0,  // pevna farba
  SCHEME_WAKEUP      = 1,  // dychovy cyklus - plynuly nabeh a nasledne dostmievanie jasu
  SCHEME_CYCLE_UP    = 2,  // plynule prechadzanie farieb hore
  SCHEME_CYCLE_DOWN  = 3,  // plynule prechadzanie farieb dole
  SCHEME_RANDOM      = 4,  // nahodne farby s prechodom
  SCHEME_CANDLE      = 6,  // plamen sviecky (flicker)
  SCHEME_RGB_PATTERN = 7,  // striedanie R/G/B blokov
  SCHEME_RAINBOW     = 11  // pohybujuca sa duha
};

// Poradie, cez ktore 2x klik prepina rezimy
extern const uint8_t SCHEME_ORDER[];
extern const uint8_t SCHEME_ORDER_LEN;

void ledBegin();
void ledUpdate();                 // volat v loop() - nonblocking
void ledApplyPower();             // ihned vypne/rozsvieti podla cfg.power
void ledNextScheme();             // posunie na dalsi rezim v SCHEME_ORDER
void ledAdjustBrightness(int8_t delta);
