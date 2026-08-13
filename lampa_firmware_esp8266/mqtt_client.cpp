#include "mqtt_client.h"
#include "config.h"
#include "led_control.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Domoticz MQTT protokol:
//  - prichadzajuce prikazy: subscribe "domoticz/out", JSON {"idx":X,"nvalue":0/1,...}
//  - odchadzajuci stav:     publish  "domoticz/in",  JSON {"idx":X,"nvalue":0/1}

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static uint32_t lastReconnectAttempt = 0;

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  Serial.printf("[MQTT] prijata sprava na '%s' (%u B)\n", topic, len);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) {
    Serial.printf("[MQTT] chyba parsovania JSON: %s\n", err.c_str());
    return;
  }
  if (!doc.containsKey("idx")) { Serial.println("[MQTT] sprava neobsahuje 'idx', ignorujem"); return; }

  int msgIdx = doc["idx"].as<int>();
  if (msgIdx != cfg.domoticzIdx) {
    Serial.printf("[MQTT] idx %d nesedi s nastavenym %d, ignorujem\n", msgIdx, cfg.domoticzIdx);
    return;
  }
  if (!doc.containsKey("nvalue")) { Serial.println("[MQTT] sprava neobsahuje 'nvalue', ignorujem"); return; }

  cfg.power = (doc["nvalue"].as<int>() != 0);
  Serial.printf("[MQTT] idx %d zhoda -> power = %d\n", msgIdx, cfg.power);
  ledApplyPower();
  configSave();
}

void mqttBegin() {
  if (strlen(cfg.mqttHost) == 0) return;
  // Domoticz JSON spravy na "domoticz/out" byvaju vacsie ako standardny PubSubClient
  // limit 256 B (obsahuju idx, name, dtype, battery, rssi, LastUpdate a dalsie polia) -
  // bez tohto by sa dlhsie spravy potichu zahadzovali a ovladanie z Domoticz by nefungovalo.
  mqtt.setBufferSize(512);
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
  if (strlen(cfg.mqttHost) == 0) return; // MQTT zatial nenastavene

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
