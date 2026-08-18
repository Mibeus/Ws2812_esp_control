#include "led_control.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define PIN_WS2812 4

static Adafruit_NeoPixel *strip = nullptr;

const uint8_t SCHEME_ORDER[] = {
  SCHEME_SINGLE, SCHEME_WAKEUP, SCHEME_CYCLE_UP, SCHEME_CYCLE_DOWN,
  SCHEME_RANDOM, SCHEME_CANDLE, SCHEME_RGB_PATTERN, SCHEME_RAINBOW
};
const uint8_t SCHEME_ORDER_LEN = sizeof(SCHEME_ORDER) / sizeof(SCHEME_ORDER[0]);

static uint32_t lastStep = 0;
static uint16_t animPos = 0;
static uint32_t wakeupStartMs = 0;
static uint8_t candleLevel = 220;
static uint8_t lastWakeupLevel = 0; // posledny zobrazeny jas vo Wakeup rezime (0-255), pre plynuly nadvazujuci fade-out

// stav plynuleho zhasinania (len pre Wakeup rezim, spusti sa pri vypnuti)
static bool fadingOut = false;
static uint32_t fadeOutStartMs = 0;
static uint8_t fadeOutFromLevel = 0;

static const uint32_t WAKEUP_FADE_MS = 4000; // dlzka plynuleho nabehu/zhasnutia

static float easeInOut(float phase) { // phase 0..1 -> 0..1, sinusovy ease (bez lomu na koncoch)
  if (phase < 0) phase = 0;
  if (phase > 1) phase = 1;
  return 0.5f - 0.5f * cosf(phase * (float)PI);
}

static uint32_t hsv(uint8_t h, uint8_t s, uint8_t v) {
  return strip->gamma32(strip->ColorHSV((uint16_t)h * 257, s, v));
}

void ledBegin() {
  strip = new Adafruit_NeoPixel(cfg.ledCount, PIN_WS2812, NEO_GRB + NEO_KHZ800);
  strip->begin();
  strip->show();
}

void ledApplyPower() {
  if (!cfg.power) {
    if (cfg.scheme == SCHEME_WAKEUP && lastWakeupLevel > 0) {
      // plynule zhasnutie namiesto okamziteho vypnutia - pokracuje presne odtial,
      // kde bol jas v momente vypnutia (aj ked bol este uprostred nabehu)
      fadingOut = true;
      fadeOutStartMs = millis();
      fadeOutFromLevel = lastWakeupLevel;
    } else {
      strip->clear();
      strip->show();
    }
  } else {
    wakeupStartMs = 0; // zacne novy plynuly nabeh od nuly
    fadingOut = false;
  }
}

void ledNextScheme() {
  uint8_t idx = 0;
  for (uint8_t i = 0; i < SCHEME_ORDER_LEN; i++) {
    if (SCHEME_ORDER[i] == cfg.scheme) { idx = i; break; }
  }
  idx = (idx + 1) % SCHEME_ORDER_LEN;
  cfg.scheme = SCHEME_ORDER[idx];
  animPos = 0;
  wakeupStartMs = 0;
  configSave();
}

void ledAdjustBrightness(int8_t delta) {
  int v = (int)cfg.brightness + delta;
  if (v < 5) v = 5;     // nikdy do uplnej tmy pocas drzania, nech je vidno ze lampa reaguje
  if (v > 255) v = 255;
  cfg.brightness = (uint8_t)v;
}

static void renderSingle() {
  uint32_t c = hsv(cfg.hue, cfg.saturation, cfg.brightness);
  for (uint16_t i = 0; i < strip->numPixels(); i++) strip->setPixelColor(i, c);
}

static void renderWakeup() {
  // Jednorazovy plynuly nabeh na nastaveny jas pri zapnuti, potom zostane staticky.
  // Zhasnutie (plynuly fade-out) rieši ledApplyPower()/ledUpdate() nizsie, mimo tejto funkcie.
  if (wakeupStartMs == 0) wakeupStartMs = millis(); // prvy vstup do rezimu / cerstve zapnutie

  uint32_t elapsed = millis() - wakeupStartMs;
  float level = (elapsed >= WAKEUP_FADE_MS) ? 1.0f : easeInOut((float)elapsed / (float)WAKEUP_FADE_MS);

  lastWakeupLevel = (uint8_t)(level * cfg.brightness);
  uint32_t c = hsv(cfg.hue, cfg.saturation, lastWakeupLevel);
  for (uint16_t i = 0; i < strip->numPixels(); i++) strip->setPixelColor(i, c);
}

static void renderCycle(bool up) {
  uint32_t interval = map(cfg.speed, 1, 20, 200, 15);
  if (millis() - lastStep >= interval) {
    lastStep = millis();
    animPos = (animPos + (up ? 1 : 255)) & 0xFF;
  }
  uint32_t c = hsv((uint8_t)animPos, cfg.saturation, cfg.brightness);
  for (uint16_t i = 0; i < strip->numPixels(); i++) strip->setPixelColor(i, c);
}

static void renderRandom() {
  static uint8_t curHue = 0, targetHue = 0;
  uint32_t interval = map(cfg.speed, 1, 20, 1500, 200);
  if (millis() - lastStep >= interval) {
    lastStep = millis();
    targetHue = random(0, 256);
  }
  if (curHue != targetHue) curHue += (curHue < targetHue) ? 1 : -1;
  uint32_t c = hsv(curHue, cfg.saturation, cfg.brightness);
  for (uint16_t i = 0; i < strip->numPixels(); i++) strip->setPixelColor(i, c);
}

static void renderCandle() {
  // tepla oranzova s nahodnym blikanim jasu - "plamen sviecky"
  if (millis() - lastStep >= 40) {
    lastStep = millis();
    candleLevel = 190 + random(0, 66); // 190-255
  }
  for (uint16_t i = 0; i < strip->numPixels(); i++) {
    uint8_t flicker = candleLevel - random(0, 35);
    uint8_t v = (uint8_t)(((uint16_t)flicker * cfg.brightness) / 255);
    strip->setPixelColor(i, hsv(25, 255, v)); // hue 25 = tepla oranzova
  }
}

static void renderRgbPattern() {
  uint32_t interval = map(cfg.speed, 1, 20, 300, 40);
  if (millis() - lastStep >= interval) {
    lastStep = millis();
    animPos = (animPos + 1) % 3;
  }
  uint32_t colors[3] = {
    strip->Color(cfg.brightness, 0, 0),
    strip->Color(0, cfg.brightness, 0),
    strip->Color(0, 0, cfg.brightness)
  };
  for (uint16_t i = 0; i < strip->numPixels(); i++) {
    strip->setPixelColor(i, colors[(i + animPos) % 3]);
  }
}

static void renderRainbow() {
  uint32_t interval = map(cfg.speed, 1, 20, 60, 5);
  if (millis() - lastStep >= interval) {
    lastStep = millis();
    animPos = (animPos + 1) % 256;
  }
  uint16_t n = strip->numPixels();
  for (uint16_t i = 0; i < n; i++) {
    uint8_t h = (uint8_t)(((i * 256 / n) + animPos) & 0xFF);
    strip->setPixelColor(i, hsv(h, cfg.saturation, cfg.brightness));
  }
}

void ledUpdate() {
  if (fadingOut) {
    uint32_t elapsed = millis() - fadeOutStartMs;
    if (elapsed >= WAKEUP_FADE_MS) {
      fadingOut = false;
      strip->clear();
      strip->show();
      return;
    }
    float level = 1.0f - easeInOut((float)elapsed / (float)WAKEUP_FADE_MS); // 1 -> 0
    uint8_t v = (uint8_t)(level * fadeOutFromLevel);
    uint32_t c = hsv(cfg.hue, cfg.saturation, v);
    for (uint16_t i = 0; i < strip->numPixels(); i++) strip->setPixelColor(i, c);
    strip->show();
    return;
  }

  if (!cfg.power) return;

  switch (cfg.scheme) {
    case SCHEME_SINGLE:      renderSingle();      break;
    case SCHEME_WAKEUP:      renderWakeup();      break;
    case SCHEME_CYCLE_UP:    renderCycle(true);   break;
    case SCHEME_CYCLE_DOWN:  renderCycle(false);  break;
    case SCHEME_RANDOM:      renderRandom();      break;
    case SCHEME_CANDLE:      renderCandle();      break;
    case SCHEME_RGB_PATTERN: renderRgbPattern();  break;
    case SCHEME_RAINBOW:     renderRainbow();     break;
    default:                 renderSingle();      break;
  }
  strip->show();
}
