#include "mqtt_client.h"
#include "config.h"
#include "pins.h"
#include "effects_pwm.h"
#include "effects_ws2812.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ctype.h>

// Domoticz MQTT protokol - lisi sa podla funkcie slotu:
//  - FUNC_ONOFF (plain Switch):     {"idx":X,"nvalue":0/1}
//  - FUNC_PWM (Dimmer):             prichadzajuce {"idx":X,"nvalue":0/1,"svalue1":"<0-100%>"}
//                                    odchadzajuce  {"idx":X,"nvalue":0/1,"svalue":"<0-100%>"}
//  - FUNC_WS2812 (Color Switch):    prichadzajuce {"idx":X,"nvalue":..,"Level":0-100,"Color":{...}}
//                                    odchadzajuce  {"idx":X,"nvalue":0/1,"svalue":"<0-100%>","Color":{...}}
//  - Volitelny Selector Switch (WS2812/PWM scheme): {"idx":Y,"nvalue":nasobky 10}

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static uint32_t lastReconnectAttempt = 0;

// Maly webovy log poslednych MQTT udalosti - dostupny cez /debug, nahradza Serial monitor
#define MQTT_LOG_SIZE 6
static String mqttLog[MQTT_LOG_SIZE];
static uint8_t mqttLogPos = 0;

static void logMqtt(const String &line) {
  mqttLog[mqttLogPos] = String(millis() / 1000) + "s: " + line;
  mqttLogPos = (mqttLogPos + 1) % MQTT_LOG_SIZE;
  Serial.println("[MQTT] " + line);
}

String mqttDebugLog() {
  String out;
  for (uint8_t i = 0; i < MQTT_LOG_SIZE; i++) {
    uint8_t idx = (mqttLogPos + i) % MQTT_LOG_SIZE;
    if (mqttLog[idx].length() > 0) out += mqttLog[idx] + "<br>";
  }
  if (out.length() == 0) out = "(zatial ziadna udalost)";
  return out;
}

// MQTT client ID nesmie obsahovat medzery ani specialne znaky - niektore
// brokery sa s takym clientId vedia spravat nepredvidatelne.
static String sanitizeClientId(const String &name) {
  String out;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_') out += c;
  }
  if (out.length() == 0) out = "device";
  return out;
}

static void rgbToHueSat(uint8_t r, uint8_t g, uint8_t b, uint8_t &hueOut, uint8_t &satOut) {
  float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
  float maxc = max(rf, max(gf, bf));
  float minc = min(rf, min(gf, bf));
  float delta = maxc - minc;
  float h = 0;
  if (delta > 0.0001f) {
    if (maxc == rf)      h = fmodf(((gf - bf) / delta), 6.0f);
    else if (maxc == gf) h = ((bf - rf) / delta) + 2.0f;
    else                 h = ((rf - gf) / delta) + 4.0f;
    h *= 60.0f;
    if (h < 0) h += 360.0f;
  }
  float s = (maxc <= 0.0001f) ? 0 : (delta / maxc);
  hueOut = (uint8_t)(h / 360.0f * 255.0f);
  satOut = (uint8_t)(s * 255.0f);
}

static void hueSatValToRgb(uint8_t hue, uint8_t sat, uint8_t val, uint8_t &r, uint8_t &g, uint8_t &b) {
  float h = hue / 255.0f * 360.0f;
  float s = sat / 255.0f;
  float v = val / 255.0f;
  float c = v * s;
  float x = c * (1 - fabsf(fmodf(h / 60.0f, 2.0f) - 1));
  float m = v - c;
  float rf, gf, bf;
  if (h < 60)       { rf = c; gf = x; bf = 0; }
  else if (h < 120) { rf = x; gf = c; bf = 0; }
  else if (h < 180) { rf = 0; gf = c; bf = x; }
  else if (h < 240) { rf = 0; gf = x; bf = c; }
  else if (h < 300) { rf = x; gf = 0; bf = c; }
  else              { rf = c; gf = 0; bf = x; }
  r = (uint8_t)((rf + m) * 255);
  g = (uint8_t)((gf + m) * 255);
  b = (uint8_t)((bf + m) * 255);
}

static void handleIncomingForSlot(uint8_t slot, JsonDocument &doc) {
  SlotConfig &sl = cfg.slots[slot];

  if (sl.function == FUNC_ONOFF) {
    if (doc.containsKey("nvalue")) {
      sl.power = (doc["nvalue"].as<int>() != 0);
      pinsApplyPower(slot);
      configSave();
    }
    return;
  }

  if (sl.function == FUNC_PWM) {
    bool changed = false;
    if (doc.containsKey("nvalue")) { sl.power = (doc["nvalue"].as<int>() != 0); changed = true; }
    if (doc.containsKey("svalue1")) {
      int level = String((const char*)(doc["svalue1"] | "")).toInt();
      if (level > 0) { sl.brightness = (uint8_t)constrain(level * 255 / 100, 0, 255); changed = true; }
    }
    if (changed) { pinsApplyPower(slot); configSave(); }
    return;
  }

  if (sl.function == FUNC_WS2812) {
    bool changed = false;
    if (doc.containsKey("Color")) {
      JsonObject c = doc["Color"];
      uint8_t r = c["r"] | 0, g = c["g"] | 0, b = c["b"] | 0;
      uint8_t cw = c["cw"] | 0, ww = c["ww"] | 0;
      if (r == 0 && g == 0 && b == 0 && (cw > 0 || ww > 0)) {
        sl.saturation = 0;
      } else {
        uint8_t hue, sat;
        rgbToHueSat(r, g, b, hue, sat);
        sl.hue = hue;
        sl.saturation = sat;
      }
      sl.scheme = WS_SCHEME_SINGLE;
      changed = true;
    }
    if (doc.containsKey("Level")) {
      int level = doc["Level"].as<int>();
      sl.brightness = (uint8_t)constrain(level * 255 / 100, 0, 255);
      changed = true;
    }
    if (doc.containsKey("nvalue")) { sl.power = (doc["nvalue"].as<int>() != 0); changed = true; }
    if (changed) { pinsApplyPower(slot); configSave(); }
    return;
  }
}

static void handleSchemeForSlot(uint8_t slot, JsonDocument &doc) {
  SlotConfig &sl = cfg.slots[slot];
  if (sl.function != FUNC_WS2812) return; // Domoticz vyber rezimu riesime len pre WS2812
  if (!doc.containsKey("nvalue")) return;
  int level = doc["nvalue"].as<int>();
  uint8_t i = (uint8_t)constrain(level / 10, 0, WS_SCHEME_ORDER_LEN - 1);
  sl.scheme = WS_SCHEME_ORDER[i];
  configSave();
}

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  String payloadStr;
  for (unsigned int i = 0; i < len && i < 150; i++) payloadStr += (char)payload[i];
  logMqtt("RX " + String(topic) + " (" + String(len) + "B): " + payloadStr);

  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, payload, len)) { logMqtt("chyba parsovania JSON"); return; }
  if (!doc.containsKey("idx")) { logMqtt("sprava bez 'idx', ignorujem"); return; }
  int msgIdx = doc["idx"].as<int>();

  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    if (cfg.slots[i].domoticzIdx != 0 && cfg.slots[i].domoticzIdx == msgIdx) {
      logMqtt("zhoda idx " + String(msgIdx) + " -> slot " + String(i) + " (hlavne zariadenie)");
      handleIncomingForSlot(i, doc);
      return;
    }
    if (cfg.slots[i].domoticzSchemeIdx != 0 && cfg.slots[i].domoticzSchemeIdx == msgIdx) {
      logMqtt("zhoda idx " + String(msgIdx) + " -> slot " + String(i) + " (selector rezimu)");
      handleSchemeForSlot(i, doc);
      return;
    }
  }
  logMqtt("idx " + String(msgIdx) + " nesedi so ziadnym nastavenym slotom");
}

void mqttBegin() {
  if (strlen(cfg.mqttHost) == 0) return;
  mqtt.setBufferSize(1024);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setCallback(onMqttMessage);
}

static bool mqttConnect() {
  String clientId = sanitizeClientId(cfg.deviceName) + "-" + WiFi.macAddress();
  bool ok = strlen(cfg.mqttUser) > 0
              ? mqtt.connect(clientId.c_str(), cfg.mqttUser, cfg.mqttPass)
              : mqtt.connect(clientId.c_str());
  if (ok) {
    bool subOk = mqtt.subscribe("domoticz/out");
    logMqtt("pripojene (id=" + clientId + "), subscribe domoticz/out: " + (subOk ? "OK" : "ZLYHALO"));
  } else {
    logMqtt("pripojenie zlyhalo, rc=" + String(mqtt.state()));
  }
  return ok;
}

void mqttLoop() {
  if (strlen(cfg.mqttHost) == 0) return;
  if (!mqtt.connected()) {
    uint32_t now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      mqttConnect();
    }
  } else {
    mqtt.loop();
  }
}

bool mqttIsConnected() { return mqtt.connected(); }

void mqttPublishSlot(uint8_t slot) {
  if (!mqtt.connected() || slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];
  if (sl.domoticzIdx == 0) return;

  StaticJsonDocument<384> doc;
  doc["idx"] = sl.domoticzIdx;
  doc["nvalue"] = sl.power ? 1 : 0;

  if (sl.function == FUNC_ONOFF) {
    // ziadne dalsie polia
  } else if (sl.function == FUNC_PWM) {
    doc["svalue"] = String((int)((uint32_t)sl.brightness * 100 / 255));
  } else if (sl.function == FUNC_WS2812) {
    doc["svalue"] = String((int)((uint32_t)sl.brightness * 100 / 255));
    uint8_t r, g, b;
    hueSatValToRgb(sl.hue, sl.saturation, 255, r, g, b);
    JsonObject color = doc.createNestedObject("Color");
    color["m"] = 3; color["t"] = 0;
    color["r"] = r; color["g"] = g; color["b"] = b;
    color["cw"] = 0; color["ww"] = 0;
  } else {
    return;
  }

  char buf[384];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
}

void mqttPublishSlotScheme(uint8_t slot) {
  if (!mqtt.connected() || slot >= NUM_SLOTS) return;
  SlotConfig &sl = cfg.slots[slot];
  if (sl.function != FUNC_WS2812 || sl.domoticzSchemeIdx == 0) return;

  uint8_t i = 0;
  for (uint8_t k = 0; k < WS_SCHEME_ORDER_LEN; k++) if (WS_SCHEME_ORDER[k] == sl.scheme) { i = k; break; }

  StaticJsonDocument<128> doc;
  doc["idx"] = sl.domoticzSchemeIdx;
  doc["nvalue"] = i * 10;
  char buf[128];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
}
