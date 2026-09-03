#pragma once
#include <Arduino.h>

enum PwmScheme : uint8_t {
  PWM_SCHEME_STATIC = 0,
  PWM_SCHEME_WAKEUP = 1,
  PWM_SCHEME_PULSE  = 2,
  PWM_SCHEME_RANDOM = 3,
  PWM_SCHEME_CANDLE = 4
};
#define PWM_SCHEME_COUNT 5

void pwmBeginSlot(uint8_t slot);
void pwmUpdateSlot(uint8_t slot);
void pwmApplyPower(uint8_t slot);
void pwmNextScheme(uint8_t slot);

bool pwmSlotAttached(uint8_t slot);
uint8_t pwmSlotLastDuty(uint8_t slot);
