#include "button.h"

// TTP223 modul (momentalny/direct mod - default z vyroby): HIGH pocas dotyku, LOW pustene.
// Ak mas modul prepojeny do "toggle" modu, treba tuto logiku upravit.

static uint8_t btnPin;
static bool lastRaw = false;
static bool debounced = false;
static uint32_t lastChangeTime = 0;
static const uint32_t DEBOUNCE_MS = 30;

static uint32_t pressStart = 0;
static bool isHolding = false;
static int8_t holdDirection = 1;
static const uint32_t HOLD_THRESHOLD_MS = 400;
static const uint32_t HOLD_STEP_MS = 60;
static uint32_t lastHoldStep = 0;

static uint8_t clickCount = 0;
static uint32_t lastReleaseTime = 0;
static const uint32_t MULTI_CLICK_WINDOW_MS = 350;

static ButtonSingleCb onSingle = nullptr;
static ButtonDoubleCb onDouble = nullptr;
static ButtonTripleCb onTriple = nullptr;
static ButtonHoldCb onHold = nullptr;
static ButtonHoldEndCb onHoldEnd = nullptr;

void buttonBegin(uint8_t pin) {
  btnPin = pin;
  pinMode(btnPin, INPUT); // TTP223 ma vlastny push-pull vystup, netreba interny pull-up
}

void buttonSetCallbacks(ButtonSingleCb s, ButtonDoubleCb d, ButtonTripleCb t,
                         ButtonHoldCb h, ButtonHoldEndCb he) {
  onSingle = s; onDouble = d; onTriple = t; onHold = h; onHoldEnd = he;
}

void buttonUpdate() {
  bool raw = digitalRead(btnPin) == HIGH;

  if (raw != lastRaw) {
    lastChangeTime = millis();
    lastRaw = raw;
  }

  if ((millis() - lastChangeTime) > DEBOUNCE_MS && raw != debounced) {
    debounced = raw;

    if (debounced) {
      // ---- zaciatok dotyku ----
      pressStart = millis();
      isHolding = false;
    } else {
      // ---- koniec dotyku ----
      uint32_t heldFor = millis() - pressStart;
      if (isHolding) {
        holdDirection = -holdDirection; // priste drzanie pojde opacnym smerom
        isHolding = false;
        if (onHoldEnd) onHoldEnd();
      } else if (heldFor < HOLD_THRESHOLD_MS) {
        clickCount++;
        lastReleaseTime = millis();
      }
    }
  }

  // prechod do rezimu "drzanie"
  if (debounced && !isHolding && (millis() - pressStart) >= HOLD_THRESHOLD_MS) {
    isHolding = true;
    lastHoldStep = millis();
  }
  if (debounced && isHolding && (millis() - lastHoldStep) >= HOLD_STEP_MS) {
    lastHoldStep = millis();
    if (onHold) onHold(holdDirection);
  }

  // vyhodnotenie 1x / 2x / 3x klik po uplynuti casoveho okna
  if (clickCount > 0 && (millis() - lastReleaseTime) > MULTI_CLICK_WINDOW_MS) {
    if (clickCount == 1 && onSingle) onSingle();
    else if (clickCount == 2 && onDouble) onDouble();
    else if (clickCount >= 3 && onTriple) onTriple();
    clickCount = 0;
  }
}
