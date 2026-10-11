#include "event_log.h"
#include "config.h"
#include <stdarg.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  extern "C" {
    #include "user_interface.h"
  }
#else
  #include <WiFi.h>
  #include <esp_system.h>
#endif

#define LOG_TEXT_LEN 52
#define LOG_RING     80
#define LOG_RTC_N    8

struct LogEntry {
  uint32_t ms;
  char text[LOG_TEXT_LEN];
};

static LogEntry ring[LOG_RING];
static uint16_t ringCount = 0;   // kolko platnych zaznamov (max LOG_RING)
static uint16_t ringHead = 0;    // kam sa zapise dalsi

// ---- RTC zaloha (prezije software restart / reset, nie vypadok napajania) ----
struct RtcLog {
  uint32_t magic;
  uint32_t crc;       // crc cez vsetko za tymto polom
  uint32_t boots;
  uint32_t lastMs;    // posledny znamy uptime (heartbeat + kazdy log)
  uint32_t n;         // pocet platnych zaznamov v e[]
  LogEntry e[LOG_RTC_N];
};
static const uint32_t RTC_MAGIC = 0x4C4F4731; // "LOG1"

#ifndef ESP8266
static RTC_NOINIT_ATTR RtcLog rtcStore;
#endif
static RtcLog rtc;                 // pracovna kopia

static LogEntry prevEntries[LOG_RTC_N];
static uint32_t prevCount = 0;
static uint32_t prevLastMs = 0;
static bool prevValid = false;
static uint32_t bootCount = 1;
static String resetReasonText;

static uint32_t crc32(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  while (n--) {
    c ^= *d++;
    for (uint8_t i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320UL & (-(int32_t)(c & 1)));
  }
  return ~c;
}

static void rtcSave() {
  rtc.magic = RTC_MAGIC;
  rtc.crc = crc32((const uint8_t*)&rtc.boots, sizeof(RtcLog) - 2 * sizeof(uint32_t));
#ifdef ESP8266
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc, sizeof(RtcLog));
#else
  memcpy(&rtcStore, &rtc, sizeof(RtcLog));
#endif
}

static bool rtcLoad(RtcLog &out) {
#ifdef ESP8266
  if (!ESP.rtcUserMemoryRead(0, (uint32_t*)&out, sizeof(RtcLog))) return false;
#else
  memcpy(&out, &rtcStore, sizeof(RtcLog));
#endif
  if (out.magic != RTC_MAGIC) return false;
  if (out.n > LOG_RTC_N) return false;
  return out.crc == crc32((const uint8_t*)&out.boots, sizeof(RtcLog) - 2 * sizeof(uint32_t));
}

String logResetReason() { return resetReasonText; }
uint32_t logBootCount() { return bootCount; }

static bool wasPowerOn() {
#ifdef ESP8266
  return ESP.getResetInfoPtr()->reason == REASON_DEFAULT_RST;
#else
  return esp_reset_reason() == ESP_RST_POWERON;
#endif
}

static void pushRtc() {
  // poslednych LOG_RTC_N zaznamov z ringu (od najstarsieho po najnovsi)
  uint16_t take = ringCount < LOG_RTC_N ? ringCount : LOG_RTC_N;
  for (uint16_t i = 0; i < take; i++) {
    uint16_t idx = (ringHead + LOG_RING - take + i) % LOG_RING;
    rtc.e[i] = ring[idx];
  }
  rtc.n = take;
  rtc.lastMs = millis();
  rtcSave();
}

void logEvent(const char* fmt, ...) {
  LogEntry &e = ring[ringHead];
  e.ms = millis();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.text, LOG_TEXT_LEN, fmt, ap);
  va_end(ap);
  Serial.printf("[LOG] %s\n", e.text);
  ringHead = (ringHead + 1) % LOG_RING;
  if (ringCount < LOG_RING) ringCount++;
  pushRtc();
}

void logBegin() {
#ifdef ESP8266
  resetReasonText = ESP.getResetReason();
#else
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   resetReasonText = "zapnutie napajania"; break;
    case ESP_RST_SW:        resetReasonText = "softverovy restart"; break;
    case ESP_RST_PANIC:     resetReasonText = "pad programu (panic)"; break;
    case ESP_RST_INT_WDT:   resetReasonText = "watchdog (prerusenia)"; break;
    case ESP_RST_TASK_WDT:  resetReasonText = "watchdog (uloha)"; break;
    case ESP_RST_WDT:       resetReasonText = "watchdog"; break;
    case ESP_RST_BROWNOUT:  resetReasonText = "podpatie (brownout)"; break;
    default:                resetReasonText = "ine (" + String((int)esp_reset_reason()) + ")"; break;
  }
#endif

  RtcLog old;
  bool valid = !wasPowerOn() && rtcLoad(old);
  memset(&rtc, 0, sizeof(rtc));
  if (valid) {
    bootCount = old.boots + 1;
    prevCount = old.n;
    prevLastMs = old.lastMs;
    for (uint32_t i = 0; i < old.n; i++) prevEntries[i] = old.e[i];
    prevValid = true;
  } else {
    bootCount = 1;
  }
  rtc.boots = bootCount;
  rtcSave();

  logEvent("START v%s, dovod: %s, boot #%lu", FW_VERSION, resetReasonText.c_str(), (unsigned long)bootCount);
  if (prevValid) logEvent("predch. beh trval ~%lu s", (unsigned long)(prevLastMs / 1000));
#ifdef ESP8266
  const rst_info* ri = ESP.getResetInfoPtr();
  if (ri->reason == REASON_EXCEPTION_RST || ri->reason == REASON_WDT_RST || ri->reason == REASON_SOFT_WDT_RST) {
    logEvent("exc=%u epc1=0x%x", (unsigned)ri->exccause, (unsigned)ri->epc1);
  }
#endif
}

// ---------------------------------------------------------------------
// Zdroj zmeny + sledovanie cfg
// ---------------------------------------------------------------------
static const char* g_src = "?";
void logSetSource(const char* src) { g_src = src; }

static bool wInit = false;
// "videne" hodnoty (na detekciu zmeny) a "zalogovane" hodnoty (na vypis rozdielu)
static bool sPower; static uint8_t sScheme, sBr, sHue, sSat, sSpeed;
static uint8_t gBr, gHue, gSat, gSpeed;
static bool pendingOther = false;
static uint32_t lastOtherChange = 0;
static const char* pendSrc = "?";

void logWatchConfig() {
  if (!wInit) {
    sPower = cfg.power; sScheme = cfg.scheme;
    sBr = gBr = cfg.brightness; sHue = gHue = cfg.hue; sSat = gSat = cfg.saturation; sSpeed = gSpeed = cfg.speed;
    wInit = true;
    return;
  }
  const char* src = g_src;

  if (cfg.power != sPower) {
    logEvent("vypinac: %s -> %s [%s]", sPower ? "ON" : "OFF", cfg.power ? "ON" : "OFF", src);
    sPower = cfg.power;
    g_src = "?";
  }
  if (cfg.scheme != sScheme) {
    // z MQTT logujeme iba on/off
    if (strcmp(src, "mqtt") != 0) logEvent("rezim: %u -> %u [%s]", sScheme, cfg.scheme, src);
    sScheme = cfg.scheme;
    g_src = "?";
  }

  if (cfg.brightness != sBr || cfg.hue != sHue || cfg.saturation != sSat || cfg.speed != sSpeed) {
    sBr = cfg.brightness; sHue = cfg.hue; sSat = cfg.saturation; sSpeed = cfg.speed;
    lastOtherChange = millis();
    pendingOther = true;
    if (strcmp(src, "?") != 0) pendSrc = src;
  }
  // jas/farba sa loguju az ked sa hodnoty 0,8 s nemenia (inak by plynule drzanie / slider zahltili log)
  if (pendingOther && millis() - lastOtherChange > 800) {
    pendingOther = false;
    if (strcmp(pendSrc, "mqtt") != 0) {
      logEvent("jas %u->%u odt %u->%u syt %u->%u rych %u->%u [%s]",
               gBr, sBr, gHue, sHue, gSat, sSat, gSpeed, sSpeed, pendSrc);
    }
    gBr = sBr; gHue = sHue; gSat = sSat; gSpeed = sSpeed;
    pendSrc = "?";
    g_src = "?";
  }
}

void logHeartbeat() {
  static uint32_t lastBeat = 0;
  static uint32_t lastRtc = 0;
  uint32_t now = millis();
  if (now - lastRtc > 10000) { lastRtc = now; pushRtc(); }
  if (now - lastBeat < 15UL * 60UL * 1000UL) return;
  lastBeat = now;
  logEvent("stav: rssi %d, heap %u, p=%d r=%u", (int)WiFi.RSSI(), (unsigned)ESP.getFreeHeap(), cfg.power ? 1 : 0, cfg.scheme);
}

// ---------------------------------------------------------------------
// Vypis
// ---------------------------------------------------------------------
static void fmtTime(char* out, size_t n, uint32_t ms) {
  uint32_t s = ms / 1000;
  snprintf(out, n, "%02u:%02u:%02u.%03u", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60), (unsigned)(ms % 1000));
}

void logForEach(LogLineCb cb, void* ctx) {
  char line[96];
  char t[16];
  for (uint16_t i = 0; i < ringCount; i++) {
    uint16_t idx = (ringHead + LOG_RING - 1 - i) % LOG_RING;
    fmtTime(t, sizeof(t), ring[idx].ms);
    snprintf(line, sizeof(line), "%s  %s", t, ring[idx].text);
    cb(line, false, ctx);
  }
  if (prevValid) {
    for (int32_t i = (int32_t)prevCount - 1; i >= 0; i--) {
      fmtTime(t, sizeof(t), prevEntries[i].ms);
      snprintf(line, sizeof(line), "%s  %s", t, prevEntries[i].text);
      cb(line, true, ctx);
    }
  }
}
