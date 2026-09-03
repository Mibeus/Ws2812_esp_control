#include "effects_ws2812.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

static Adafruit_NeoPixel *strips[NUM_SLOTS] = {nullptr, nullptr, nullptr, nullptr};

const uint8_t WS_SCHEME_ORDER[] = {
  WS_SCHEME_SINGLE, WS_SCHEME_WAKEUP, WS_SCHEME_CYCLE_UP, WS_SCHEME_CYCLE_DOWN,
  WS_SCHEME_RANDOM, WS_SCHEME_CANDLE, WS_SCHEME_RGB_PATTERN, WS_SCHEME_RAINBOW
};
const uint8_t WS_SCHEME_ORDER_LEN = sizeof(WS_SCHEME_ORDER) / sizeof(WS_SCHEME_ORDER[0]);

static uint32_t lastStep[NUM_SLOTS]      = {0, 0, 0, 0};
static uint16_t animPos[NUM_SLOTS]       = {0, 0, 0, 0};
static uint32_t wakeupStartMs[NUM_SLOTS] = {0, 0, 0, 0};
static uint8_t  candleLevel[NUM_SLOTS]   = {220, 220, 220, 220};
static uint8_t  lastWakeupLevel[NUM_SLOTS] = {0, 0, 0, 0};

static bool     fadingOut[NUM_SLOTS]       = {false, false, false, false};
static uint32_t fadeOutStartMs[NUM_SLOTS]  = {0, 0, 0, 0};
static uint8_t  fadeOutFromLevel[NUM_SLOTS] = {0, 0, 0, 0};

static const uint32_t WAKEUP_FADE_MS = 4000;

static float easeInOut(float phase) {
  if (phase < 0) phase = 0;
  if (phase > 1) phase = 1;
  return 0.5f - 0.5f * cosf(phase * (float)PI);
}

static uint32_t hsv(uint8_t slot, uint8_t h, uint8_t s, uint8_t v) {
  return strips[slot]->gamma32(strips[slot]->ColorHSV((uint16_t)h * 257, s, v));
}

void ws2812BeginSlot(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];
  if (strips[slot]) { delete strips[slot]; strips[slot] = nullptr; }
  strips[slot] = new Adafruit_NeoPixel(sl.ledCount, sl.gpio, NEO_GRB + NEO_KHZ800);
  strips[slot]->begin();
  strips[slot]->show();
}

void ws2812ApplyPower(uint8_t slot) {
  if (slot >= NUM_SLOTS || !strips[slot]) return;
  SlotConfig &sl = cfg.slots[slot];
  if (!sl.power) {
    if (sl.scheme == WS_SCHEME_WAKEUP && lastWakeupLevel[slot] > 0) {
      fadingOut[slot] = true;
      fadeOutStartMs[slot] = millis();
      fadeOutFromLevel[slot] = lastWakeupLevel[slot];
    } else {
      strips[slot]->clear();
      strips[slot]->show();
    }
  } else {
    wakeupStartMs[slot] = 0;
    fadingOut[slot] = false;
  }
}

void ws2812NextScheme(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];
  uint8_t idx = 0;
  for (uint8_t i = 0; i < WS_SCHEME_ORDER_LEN; i++) if (WS_SCHEME_ORDER[i] == sl.scheme) { idx = i; break; }
  idx = (idx + 1) % WS_SCHEME_ORDER_LEN;
  sl.scheme = WS_SCHEME_ORDER[idx];
  animPos[slot] = 0;
  wakeupStartMs[slot] = 0;
  configSave();
}

void ws2812AdjustBrightness(uint8_t slot, int8_t delta) {
  if (slot >= NUM_SLOTS) return;
  int v = (int)cfg.slots[slot].brightness + delta;
  if (v < 5) v = 5;
  if (v > 255) v = 255;
  cfg.slots[slot].brightness = (uint8_t)v;
}

static void renderSingle(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  uint32_t c = hsv(slot, sl.hue, sl.saturation, sl.brightness);
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) strips[slot]->setPixelColor(i, c);
}

static void renderWakeup(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  if (wakeupStartMs[slot] == 0) wakeupStartMs[slot] = millis();
  uint32_t elapsed = millis() - wakeupStartMs[slot];
  float level = (elapsed >= WAKEUP_FADE_MS) ? 1.0f : easeInOut((float)elapsed / (float)WAKEUP_FADE_MS);
  lastWakeupLevel[slot] = (uint8_t)(level * sl.brightness);
  uint32_t c = hsv(slot, sl.hue, sl.saturation, lastWakeupLevel[slot]);
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) strips[slot]->setPixelColor(i, c);
}

static void renderCycle(uint8_t slot, bool up) {
  SlotConfig &sl = cfg.slots[slot];
  uint32_t interval = map(sl.speed, 1, 20, 200, 15);
  if (millis() - lastStep[slot] >= interval) {
    lastStep[slot] = millis();
    animPos[slot] = (animPos[slot] + (up ? 1 : 255)) & 0xFF;
  }
  uint32_t c = hsv(slot, (uint8_t)animPos[slot], sl.saturation, sl.brightness);
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) strips[slot]->setPixelColor(i, c);
}

static void renderRandom(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  static uint8_t curHue[NUM_SLOTS] = {0, 0, 0, 0};
  static uint8_t targetHue[NUM_SLOTS] = {0, 0, 0, 0};
  uint32_t interval = map(sl.speed, 1, 20, 1500, 200);
  if (millis() - lastStep[slot] >= interval) {
    lastStep[slot] = millis();
    targetHue[slot] = random(0, 256);
  }
  if (curHue[slot] != targetHue[slot]) curHue[slot] += (curHue[slot] < targetHue[slot]) ? 1 : -1;
  uint32_t c = hsv(slot, curHue[slot], sl.saturation, sl.brightness);
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) strips[slot]->setPixelColor(i, c);
}

static void renderCandle(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  if (millis() - lastStep[slot] >= 40) {
    lastStep[slot] = millis();
    candleLevel[slot] = 190 + random(0, 66);
  }
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) {
    uint8_t flicker = candleLevel[slot] - random(0, 35);
    uint8_t v = (uint8_t)(((uint16_t)flicker * sl.brightness) / 255);
    strips[slot]->setPixelColor(i, hsv(slot, 25, 255, v));
  }
}

static void renderRgbPattern(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  uint32_t interval = map(sl.speed, 1, 20, 300, 40);
  if (millis() - lastStep[slot] >= interval) {
    lastStep[slot] = millis();
    animPos[slot] = (animPos[slot] + 1) % 3;
  }
  uint32_t colors[3] = {
    strips[slot]->Color(sl.brightness, 0, 0),
    strips[slot]->Color(0, sl.brightness, 0),
    strips[slot]->Color(0, 0, sl.brightness)
  };
  for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) {
    strips[slot]->setPixelColor(i, colors[(i + animPos[slot]) % 3]);
  }
}

static void renderRainbow(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  uint32_t interval = map(sl.speed, 1, 20, 60, 5);
  if (millis() - lastStep[slot] >= interval) {
    lastStep[slot] = millis();
    animPos[slot] = (animPos[slot] + 1) % 256;
  }
  uint16_t n = strips[slot]->numPixels();
  for (uint16_t i = 0; i < n; i++) {
    uint8_t h = (uint8_t)(((i * 256 / n) + animPos[slot]) & 0xFF);
    strips[slot]->setPixelColor(i, hsv(slot, h, sl.saturation, sl.brightness));
  }
}

void ws2812UpdateSlot(uint8_t slot) {
  if (slot >= NUM_SLOTS || !strips[slot]) return;
  SlotConfig &sl = cfg.slots[slot];

  if (fadingOut[slot]) {
    uint32_t elapsed = millis() - fadeOutStartMs[slot];
    if (elapsed >= WAKEUP_FADE_MS) {
      fadingOut[slot] = false;
      strips[slot]->clear();
      strips[slot]->show();
      return;
    }
    float level = 1.0f - easeInOut((float)elapsed / (float)WAKEUP_FADE_MS);
    uint32_t c = hsv(slot, sl.hue, sl.saturation, (uint8_t)(level * fadeOutFromLevel[slot]));
    for (uint16_t i = 0; i < strips[slot]->numPixels(); i++) strips[slot]->setPixelColor(i, c);
    strips[slot]->show();
    return;
  }

  if (!sl.power) return;

  switch (sl.scheme) {
    case WS_SCHEME_SINGLE:      renderSingle(slot);      break;
    case WS_SCHEME_WAKEUP:      renderWakeup(slot);      break;
    case WS_SCHEME_CYCLE_UP:    renderCycle(slot, true); break;
    case WS_SCHEME_CYCLE_DOWN:  renderCycle(slot, false);break;
    case WS_SCHEME_RANDOM:      renderRandom(slot);      break;
    case WS_SCHEME_CANDLE:      renderCandle(slot);      break;
    case WS_SCHEME_RGB_PATTERN: renderRgbPattern(slot);  break;
    case WS_SCHEME_RAINBOW:     renderRainbow(slot);     break;
    default:                    renderSingle(slot);      break;
  }
  strips[slot]->show();
}
