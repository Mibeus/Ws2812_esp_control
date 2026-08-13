#include "mqtt_client.h"
#include "config.h"
#include "led_control.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>

// Domoticz MQTT protokol:
//  - prichadzajuce prikazy: subscribe "domoticz/out"
//     * jednoduchy On/Off switch:  {"idx":X,"nvalue":0/1,...}
//     * Color Switch (RGB):        {"idx":X,"nvalue":..,"Level":0-100,
//                                    "Color":{"m":..,"r":..,"g":..,"b":..,"cw":..,"ww":..},...}
//     * Selector Switch (rezimy):  {"idx":Y,"nvalue":0,10,20,30...}  (nasobky 10 = poradie urovne)
//  - odchadzajuci stav: publish "domoticz/in"
//     * {"idx":X,"nvalue":0/1,"svalue":"<jas 0-100>","Color":{...}}
//     * {"idx":Y,"nvalue":<poradie*10>}

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static uint32_t lastReconnectAttempt = 0;

// --- Prevody farieb medzi nasim internym HSV (0-255) a Domoticz RGB (0-255) ---
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

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  Serial.printf("[MQTT] prijata sprava na '%s' (%u B)\n", topic, len);

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) {
    Serial.printf("[MQTT] chyba parsovania JSON: %s\n", err.c_str());
    return;
  }
  if (!doc.containsKey("idx")) { Serial.println("[MQTT] sprava neobsahuje 'idx', ignorujem"); return; }
  int msgIdx = doc["idx"].as<int>();

  // ---- RGB / on-off zariadenie (Color Switch) ----
  if (cfg.domoticzIdx != 0 && msgIdx == cfg.domoticzIdx) {
    bool changed = false;

    if (doc.containsKey("Color")) {
      JsonObject c = doc["Color"];
      uint8_t r = c["r"] | 0, g = c["g"] | 0, b = c["b"] | 0;
      uint8_t cw = c["cw"] | 0, ww = c["ww"] | 0;
      if (r == 0 && g == 0 && b == 0 && (cw > 0 || ww > 0)) {
        cfg.saturation = 0; // biele svetlo bez farby (cw/ww kanaly) - zobrazime ako biela
        Serial.println("[MQTT] Domoticz poslal biele svetlo (cw/ww), sat=0");
      } else {
        uint8_t hue, sat;
        rgbToHueSat(r, g, b, hue, sat);
        cfg.hue = hue;
        cfg.saturation = sat;
        Serial.printf("[MQTT] Domoticz farba RGB(%u,%u,%u) -> hue=%u sat=%u\n", r, g, b, hue, sat);
      }
      cfg.scheme = SCHEME_SINGLE; // vyber konkretnej farby prepne na staticky rezim
      changed = true;
    }

    if (doc.containsKey("Level")) {
      int level = doc["Level"].as<int>();
      cfg.brightness = (uint8_t)constrain(level * 255 / 100, 0, 255);
      Serial.printf("[MQTT] Domoticz jas %d%% -> %u\n", level, cfg.brightness);
      changed = true;
    } else if (doc.containsKey("svalue1")) {
      int level = String((const char*)(doc["svalue1"] | "")).toInt();
      if (level > 0) {
        cfg.brightness = (uint8_t)constrain(level * 255 / 100, 0, 255);
        changed = true;
      }
    }

    if (doc.containsKey("nvalue")) {
      int nvalue = doc["nvalue"].as<int>();
      cfg.power = (nvalue != 0);
      Serial.printf("[MQTT] Domoticz nvalue=%d -> power=%d\n", nvalue, cfg.power);
      changed = true;
    }

    if (changed) {
      ledApplyPower();
      configSave();
    }
    return;
  }

  // ---- Selector Switch (vyber rezimu svietenia) ----
  if (cfg.domoticzSchemeIdx != 0 && msgIdx == cfg.domoticzSchemeIdx) {
    if (!doc.containsKey("nvalue")) { Serial.println("[MQTT] selector sprava bez 'nvalue', ignorujem"); return; }
    int level = doc["nvalue"].as<int>();
    uint8_t i = (uint8_t)constrain(level / 10, 0, SCHEME_ORDER_LEN - 1);
    cfg.scheme = SCHEME_ORDER[i];
    Serial.printf("[MQTT] Domoticz selector level=%d -> rezim %u\n", level, cfg.scheme);
    configSave();
    return;
  }

  Serial.printf("[MQTT] idx %d nesedi so ziadnym nastavenym zariadenim, ignorujem\n", msgIdx);
}

void mqttBegin() {
  if (strlen(cfg.mqttHost) == 0) return;
  // Domoticz Color Switch JSON spravy byvaju velke (Color objekt + vela metadat) -
  // predvoleny PubSubClient limit 256 B by ich potichu zahadzoval.
  mqtt.setBufferSize(1024);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setCallback(onMqttMessage);
}

static bool mqttConnect() {
  String clientId = "lampa-" + WiFi.macAddress();
  bool ok = strlen(cfg.mqttUser) > 0
              ? mqtt.connect(clientId.c_str(), cfg.mqttUser, cfg.mqttPass)
              : mqtt.connect(clientId.c_str());
  if (ok) {
    mqtt.subscribe("domoticz/out");
    Serial.println("[MQTT] pripojene, odoberam 'domoticz/out'");
  } else {
    Serial.printf("[MQTT] pripojenie zlyhalo, rc=%d\n", mqtt.state());
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

void mqttPublishState(bool on) {
  if (!mqtt.connected() || cfg.domoticzIdx == 0) return;
  StaticJsonDocument<128> doc;
  doc["idx"] = cfg.domoticzIdx;
  doc["nvalue"] = on ? 1 : 0;
  char buf[128];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
}

void mqttPublishColor() {
  if (!mqtt.connected() || cfg.domoticzIdx == 0) return;
  uint8_t r, g, b;
  hueSatValToRgb(cfg.hue, cfg.saturation, 255, r, g, b); // farba na plnej hodnote, jas ide zvlast cez svalue

  StaticJsonDocument<384> doc;
  doc["idx"] = cfg.domoticzIdx;
  doc["nvalue"] = cfg.power ? 1 : 0;
  doc["svalue"] = String((int)((uint32_t)cfg.brightness * 100 / 255));
  JsonObject color = doc.createNestedObject("Color");
  color["m"] = 3; // ColorModeRGB
  color["t"] = 0;
  color["r"] = r;
  color["g"] = g;
  color["b"] = b;
  color["cw"] = 0;
  color["ww"] = 0;

  char buf[384];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
  Serial.println("[MQTT] farba odoslana do domoticz/in");
}

void mqttPublishScheme() {
  if (!mqtt.connected() || cfg.domoticzSchemeIdx == 0) return;
  uint8_t i = 0;
  for (uint8_t k = 0; k < SCHEME_ORDER_LEN; k++) {
    if (SCHEME_ORDER[k] == cfg.scheme) { i = k; break; }
  }
  StaticJsonDocument<128> doc;
  doc["idx"] = cfg.domoticzSchemeIdx;
  doc["nvalue"] = i * 10;
  char buf[128];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
  Serial.println("[MQTT] rezim odoslany do domoticz/in");
}
