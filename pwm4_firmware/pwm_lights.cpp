#include "pwm_lights.h"
#include <math.h>

// Piny jednotlivych kanalov - GPIO0, GPIO1, GPIO2, GPIO3
static const uint8_t CHANNEL_PINS[NUM_CHANNELS] = {0, 1, 2, 3};

static const uint32_t PWM_FREQ = 5000;   // Hz - nad hranicou pocutelneho blikania/zvuku
static const uint8_t  PWM_RES  = 8;      // 8-bit rozlisenie (0-255), sedi s cfg.brightness

// Stav pre kazdy kanal osobitne (vsetko nezavisle)
static uint32_t animStartMs[NUM_CHANNELS]   = {0, 0, 0, 0};   // zaciatok aktualnej fazy efektu
static uint8_t  lastLevel[NUM_CHANNELS]     = {0, 0, 0, 0};   // posledny zobrazeny jas (pre plynuly fade-out)
static bool     fadingOut[NUM_CHANNELS]     = {false, false, false, false};
static uint32_t fadeOutStartMs[NUM_CHANNELS] = {0, 0, 0, 0};
static uint8_t  fadeOutFromLevel[NUM_CHANNELS] = {0, 0, 0, 0};
static uint8_t  randCurLevel[NUM_CHANNELS]  = {0, 0, 0, 0};
static uint8_t  randTargetLevel[NUM_CHANNELS] = {0, 0, 0, 0};
static uint32_t lastStepMs[NUM_CHANNELS]    = {0, 0, 0, 0};
static uint8_t  candleLevel[NUM_CHANNELS]   = {220, 220, 220, 220};

static const uint32_t FADE_MS = 4000; // dlzka plynuleho nabehu/zhasnutia (Wakeup)

static float easeInOut(float phase) {
  if (phase < 0) phase = 0;
  if (phase > 1) phase = 1;
  return 0.5f - 0.5f * cosf(phase * (float)PI);
}

void pwmLightsBegin() {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    ledcAttach(CHANNEL_PINS[i], PWM_FREQ, PWM_RES);
    ledcWrite(CHANNEL_PINS[i], 0);
  }
}

void pwmApplyPower(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return;

  if (!cfg.power[ch]) {
    if (cfg.scheme[ch] == PWM_SCHEME_WAKEUP && lastLevel[ch] > 0) {
      // plynule zhasnutie namiesto okamziteho vypnutia
      fadingOut[ch] = true;
      fadeOutStartMs[ch] = millis();
      fadeOutFromLevel[ch] = lastLevel[ch];
    } else {
      ledcWrite(CHANNEL_PINS[ch], 0);
      lastLevel[ch] = 0;
    }
  } else {
    animStartMs[ch] = 0; // zacne novy plynuly nabeh / novu fazu efektu od zaciatku
    fadingOut[ch] = false;
  }
}

void pwmNextScheme(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return;
  cfg.scheme[ch] = (cfg.scheme[ch] + 1) % PWM_SCHEME_COUNT;
  animStartMs[ch] = 0;
  configSave();
}

static void renderStatic(uint8_t ch) {
  lastLevel[ch] = cfg.brightness[ch];
  ledcWrite(CHANNEL_PINS[ch], lastLevel[ch]);
}

static void renderWakeup(uint8_t ch) {
  if (animStartMs[ch] == 0) animStartMs[ch] = millis();
  uint32_t elapsed = millis() - animStartMs[ch];
  float level = (elapsed >= FADE_MS) ? 1.0f : easeInOut((float)elapsed / (float)FADE_MS);
  lastLevel[ch] = (uint8_t)(level * cfg.brightness[ch]);
  ledcWrite(CHANNEL_PINS[ch], lastLevel[ch]);
}

static void renderPulse(uint8_t ch) {
  // opakovane plynule stmievanie hore-dole (dychanie) - toto je JEDINY efekt, kde je to zamerne
  const uint32_t half = map(cfg.speed[ch], 1, 20, 4000, 800);
  if (animStartMs[ch] == 0) animStartMs[ch] = millis();
  uint32_t t = (millis() - animStartMs[ch]) % (half * 2);
  float phase = (float)t / (float)half; // 0..2
  float level = (phase <= 1.0f) ? easeInOut(phase) : easeInOut(2.0f - phase);
  lastLevel[ch] = (uint8_t)(level * cfg.brightness[ch]);
  ledcWrite(CHANNEL_PINS[ch], lastLevel[ch]);
}

static void renderRandom(uint8_t ch) {
  uint32_t interval = map(cfg.speed[ch], 1, 20, 2500, 400);
  if (millis() - lastStepMs[ch] >= interval) {
    lastStepMs[ch] = millis();
    randTargetLevel[ch] = random(10, 256);
  }
  if (randCurLevel[ch] != randTargetLevel[ch]) {
    randCurLevel[ch] += (randCurLevel[ch] < randTargetLevel[ch]) ? 1 : -1;
  }
  lastLevel[ch] = (uint8_t)(((uint16_t)randCurLevel[ch] * cfg.brightness[ch]) / 255);
  ledcWrite(CHANNEL_PINS[ch], lastLevel[ch]);
}

static void renderCandle(uint8_t ch) {
  if (millis() - lastStepMs[ch] >= 40) {
    lastStepMs[ch] = millis();
    candleLevel[ch] = 190 + random(0, 66);
  }
  uint8_t flicker = candleLevel[ch] - random(0, 35);
  lastLevel[ch] = (uint8_t)(((uint16_t)flicker * cfg.brightness[ch]) / 255);
  ledcWrite(CHANNEL_PINS[ch], lastLevel[ch]);
}

void pwmLightsUpdate() {
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    if (fadingOut[ch]) {
      uint32_t elapsed = millis() - fadeOutStartMs[ch];
      if (elapsed >= FADE_MS) {
        fadingOut[ch] = false;
        lastLevel[ch] = 0;
        ledcWrite(CHANNEL_PINS[ch], 0);
      } else {
        float level = 1.0f - easeInOut((float)elapsed / (float)FADE_MS);
        ledcWrite(CHANNEL_PINS[ch], (uint8_t)(level * fadeOutFromLevel[ch]));
      }
      continue;
    }

    if (!cfg.power[ch]) continue;

    switch (cfg.scheme[ch]) {
      case PWM_SCHEME_STATIC: renderStatic(ch); break;
      case PWM_SCHEME_WAKEUP: renderWakeup(ch); break;
      case PWM_SCHEME_PULSE:  renderPulse(ch);  break;
      case PWM_SCHEME_RANDOM: renderRandom(ch); break;
      case PWM_SCHEME_CANDLE: renderCandle(ch); break;
      default:                renderStatic(ch); break;
    }
  }
}
