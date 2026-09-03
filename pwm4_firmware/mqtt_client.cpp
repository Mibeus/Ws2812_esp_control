#include "mqtt_client.h"
#include "config.h"
#include "pwm_lights.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Domoticz MQTT protokol pre Dimmer zariadenie:
//  - prichadzajuce: {"idx":X,"nvalue":0/1,"svalue1":"<0-100%>",...}
//  - odchadzajuce:  {"idx":X,"nvalue":0/1,"svalue":"<0-100%>"}

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static uint32_t lastReconnectAttempt = 0;

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  Serial.printf("[MQTT] prijata sprava na '%s' (%u B)\n", topic, len);

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, len)) return;
  if (!doc.containsKey("idx")) return;
  int msgIdx = doc["idx"].as<int>();

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    if (cfg.domoticzIdx[ch] == 0 || cfg.domoticzIdx[ch] != msgIdx) continue;

    bool changed = false;
    if (doc.containsKey("nvalue")) {
      int nvalue = doc["nvalue"].as<int>();
      cfg.power[ch] = (nvalue != 0);
      changed = true;
    }
    if (doc.containsKey("svalue1")) {
      int level = String((const char*)(doc["svalue1"] | "")).toInt();
      if (level > 0) {
        cfg.brightness[ch] = (uint8_t)constrain(level * 255 / 100, 0, 255);
        changed = true;
      }
    }

    if (changed) {
      Serial.printf("[MQTT] kanal %u: power=%d bright=%u\n", ch, cfg.power[ch], cfg.brightness[ch]);
      pwmApplyPower(ch);
      configSave();
    }
    return;
  }

  Serial.printf("[MQTT] idx %d nesedi so ziadnym kanalom, ignorujem\n", msgIdx);
}

void mqttBegin() {
  if (strlen(cfg.mqttHost) == 0) return;
  mqtt.setBufferSize(768);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setCallback(onMqttMessage);
}

static bool mqttConnect() {
  String clientId = String(cfg.deviceName) + "-" + WiFi.macAddress();
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

void mqttPublishChannel(uint8_t ch) {
  if (!mqtt.connected() || ch >= NUM_CHANNELS || cfg.domoticzIdx[ch] == 0) return;
  StaticJsonDocument<128> doc;
  doc["idx"] = cfg.domoticzIdx[ch];
  doc["nvalue"] = cfg.power[ch] ? 1 : 0;
  doc["svalue"] = String((int)((uint32_t)cfg.brightness[ch] * 100 / 255));
  char buf[128];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("domoticz/in", buf, n);
}
