#pragma once
#include <Arduino.h>

// Udalostny log: kruhovy buffer v RAM + zaloha poslednych 8 zaznamov v RTC pamati,
// ktora prezije restart (nie vsak uplne odpojenie napajania).

void logBegin();                         // volat hned na zaciatku setup()
void logEvent(const char* fmt, ...);     // printf-styl; cas sa doplni automaticky
void logSetSource(const char* src);      // "dotyk" / "web" / "mqtt" - kto prave meni stav
void logWatchConfig();                   // volat v loop(): zaznamena zmeny power/rezim/jas/farba
void logHeartbeat();                     // volat v loop(): raz za 15 min stavovy riadok + aktualizacia RTC

String logResetReason();                 // dovod posledneho restartu (text)
uint32_t logBootCount();

// Vykreslenie: callback dostane jeden hotovy riadok (cas + text), od najnovsieho po najstarsie
typedef void (*LogLineCb)(const char* line, bool previousBoot, void* ctx);
void logForEach(LogLineCb cb, void* ctx);
