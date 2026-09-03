#include "effects_pwm.h"
#include "config.h"
#include <math.h>

// ESP8266 nema hardverovy LEDC ako ESP32 - pouziva sa analogWrite() (software PWM).
// Rozsah prenastavime na 0-255 (analogWriteRange), aby sedel s konvenciou cfg.brightness
// a bol kod v ostatnych suboroch (mqtt_client, web_server) identicky pre obe platformy.

static bool     attachOk[NUM_SLOTS]        = {false, false, false, false};
static uint32_t animStartMs[NUM_SLOTS]     = {0, 0, 0, 0};
static uint8_t  lastLevel[NUM_SLOTS]       = {0, 0, 0, 0};
static bool     fadingOut[NUM_SLOTS]       = {false, false, false, false};
static uint32_t fadeOutStartMs[NUM_SLOTS]  = {0, 0, 0, 0};
static uint8_t  fadeOutFromLevel[NUM_SLOTS] = {0, 0, 0, 0};
static uint8_t  randCurLevel[NUM_SLOTS]    = {0, 0, 0, 0};
static uint8_t  randTargetLevel[NUM_SLOTS] = {0, 0, 0, 0};
static uint32_t lastStepMs[NUM_SLOTS]      = {0, 0, 0, 0};
static uint8_t  candleLevel[NUM_SLOTS]     = {220, 220, 220, 220};
static bool     rangeInitDone = false;

static const uint32_t FADE_MS = 4000;

static float easeInOut(float phase) {
  if (phase < 0) phase = 0;
  if (phase > 1) phase = 1;
  return 0.5f - 0.5f * cosf(phase * (float)PI);
}

void pwmBeginSlot(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  if (!rangeInitDone) {
    analogWriteRange(255); // zjednoti rozsah s ESP32 (inak by ESP8266 pouzival 0-1023)
    analogWriteFreq(1000);
    rangeInitDone = true;
  }
  uint8_t pin = cfg.slots[slot].gpio;
  pinMode(pin, OUTPUT);
  analogWrite(pin, 0);
  attachOk[slot] = true; // analogWrite nema samostatny "attach" krok, ktory by mohol zlyhat
  Serial.printf("[PWM] slot %u (GPIO%u): inicializovane (analogWrite)\n", slot, pin);
}

bool pwmSlotAttached(uint8_t slot) { return (slot < NUM_SLOTS) ? attachOk[slot] : false; }
uint8_t pwmSlotLastDuty(uint8_t slot) { return (slot < NUM_SLOTS) ? lastLevel[slot] : 0; }

void pwmApplyPower(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];
  if (!sl.power) {
    if (sl.scheme == PWM_SCHEME_WAKEUP && lastLevel[slot] > 0) {
      fadingOut[slot] = true;
      fadeOutStartMs[slot] = millis();
      fadeOutFromLevel[slot] = lastLevel[slot];
    } else {
      analogWrite(sl.gpio, 0);
      lastLevel[slot] = 0;
    }
  } else {
    animStartMs[slot] = 0;
    fadingOut[slot] = false;
  }
}

void pwmNextScheme(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  cfg.slots[slot].scheme = (cfg.slots[slot].scheme + 1) % PWM_SCHEME_COUNT;
  animStartMs[slot] = 0;
  configSave();
}

static void renderStatic(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  lastLevel[slot] = sl.brightness;
  analogWrite(sl.gpio, lastLevel[slot]);
}

static void renderWakeup(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  if (animStartMs[slot] == 0) animStartMs[slot] = millis();
  uint32_t elapsed = millis() - animStartMs[slot];
  float level = (elapsed >= FADE_MS) ? 1.0f : easeInOut((float)elapsed / (float)FADE_MS);
  lastLevel[slot] = (uint8_t)(level * sl.brightness);
  analogWrite(sl.gpio, lastLevel[slot]);
}

static void renderPulse(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  const uint32_t half = map(sl.speed, 1, 20, 4000, 800);
  if (animStartMs[slot] == 0) animStartMs[slot] = millis();
  uint32_t t = (millis() - animStartMs[slot]) % (half * 2);
  float phase = (float)t / (float)half;
  float level = (phase <= 1.0f) ? easeInOut(phase) : easeInOut(2.0f - phase);
  lastLevel[slot] = (uint8_t)(level * sl.brightness);
  analogWrite(sl.gpio, lastLevel[slot]);
}

static void renderRandom(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  uint32_t interval = map(sl.speed, 1, 20, 2500, 400);
  if (millis() - lastStepMs[slot] >= interval) {
    lastStepMs[slot] = millis();
    randTargetLevel[slot] = random(10, 256);
  }
  if (randCurLevel[slot] != randTargetLevel[slot]) {
    randCurLevel[slot] += (randCurLevel[slot] < randTargetLevel[slot]) ? 1 : -1;
  }
  lastLevel[slot] = (uint8_t)(((uint16_t)randCurLevel[slot] * sl.brightness) / 255);
  analogWrite(sl.gpio, lastLevel[slot]);
}

static void renderCandle(uint8_t slot) {
  SlotConfig &sl = cfg.slots[slot];
  if (millis() - lastStepMs[slot] >= 40) {
    lastStepMs[slot] = millis();
    candleLevel[slot] = 190 + random(0, 66);
  }
  uint8_t flicker = candleLevel[slot] - random(0, 35);
  lastLevel[slot] = (uint8_t)(((uint16_t)flicker * sl.brightness) / 255);
  analogWrite(sl.gpio, lastLevel[slot]);
}

void pwmUpdateSlot(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];

  if (fadingOut[slot]) {
    uint32_t elapsed = millis() - fadeOutStartMs[slot];
    if (elapsed >= FADE_MS) {
      fadingOut[slot] = false;
      lastLevel[slot] = 0;
      analogWrite(sl.gpio, 0);
    } else {
      float level = 1.0f - easeInOut((float)elapsed / (float)FADE_MS);
      analogWrite(sl.gpio, (uint8_t)(level * fadeOutFromLevel[slot]));
    }
    return;
  }

  if (!sl.power) return;

  switch (sl.scheme) {
    case PWM_SCHEME_STATIC: renderStatic(slot); break;
    case PWM_SCHEME_WAKEUP: renderWakeup(slot); break;
    case PWM_SCHEME_PULSE:  renderPulse(slot);  break;
    case PWM_SCHEME_RANDOM: renderRandom(slot); break;
    case PWM_SCHEME_CANDLE: renderCandle(slot); break;
    default:                renderStatic(slot); break;
  }
}
