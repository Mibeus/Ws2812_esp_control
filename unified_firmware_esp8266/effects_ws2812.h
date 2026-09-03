#pragma once
#include <Arduino.h>

// Cisla schem - zhodne s Tasmota "Scheme" prikazom (rovnake ako v povodnom WS2812 projekte)
enum Ws2812Scheme : uint8_t {
  WS_SCHEME_SINGLE      = 0,
  WS_SCHEME_WAKEUP      = 1,
  WS_SCHEME_CYCLE_UP    = 2,
  WS_SCHEME_CYCLE_DOWN  = 3,
  WS_SCHEME_RANDOM      = 4,
  WS_SCHEME_CANDLE      = 6,
  WS_SCHEME_RGB_PATTERN = 7,
  WS_SCHEME_RAINBOW     = 11
};

extern const uint8_t WS_SCHEME_ORDER[];
extern const uint8_t WS_SCHEME_ORDER_LEN;

void ws2812BeginSlot(uint8_t slot);      // vytvori NeoPixel objekt pre dany slot (podla cfg.slots[slot].gpio/ledCount)
void ws2812UpdateSlot(uint8_t slot);     // volat v loop() pre kazdy WS2812 slot
void ws2812ApplyPower(uint8_t slot);
void ws2812NextScheme(uint8_t slot);
void ws2812AdjustBrightness(uint8_t slot, int8_t delta);
