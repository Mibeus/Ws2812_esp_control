#include "config.h"
#include <Preferences.h>
#include <cstring>

Config cfg;
static Preferences prefs;

void configBegin() {
  prefs.begin("lampcfg", false);
  configLoad();
}

void configLoad() {
  String s;

  s = prefs.getString("devName", "Lampa");
  s.toCharArray(cfg.deviceName, sizeof(cfg.deviceName));

  s = prefs.getString("mqttHost", "");
  s.toCharArray(cfg.mqttHost, sizeof(cfg.mqttHost));
  cfg.mqttPort = prefs.getUShort("mqttPort", 1883);
  s = prefs.getString("mqttUser", "");
  s.toCharArray(cfg.mqttUser, sizeof(cfg.mqttUser));
  s = prefs.getString("mqttPass", "");
  s.toCharArray(cfg.mqttPass, sizeof(cfg.mqttPass));
  cfg.domoticzIdx = prefs.getInt("idx", 0);
  cfg.domoticzSchemeIdx = prefs.getInt("schemeIdx", 0);

  s = prefs.getString("otaPin", "1234");
  s.toCharArray(cfg.otaPin, sizeof(cfg.otaPin));

  cfg.ledCount = prefs.getUShort("ledCount", 30);
  cfg.brightness = prefs.getUChar("bright", 128);
  cfg.saturation = prefs.getUChar("sat", 255);
  cfg.hue = prefs.getUChar("hue", 0);
  cfg.scheme = prefs.getUChar("scheme", 0);
  cfg.power = prefs.getBool("power", true);
  cfg.speed = prefs.getUChar("speed", 8);
}

void configSave() {
  prefs.putString("devName", cfg.deviceName);

  prefs.putString("mqttHost", cfg.mqttHost);
  prefs.putUShort("mqttPort", cfg.mqttPort);
  prefs.putString("mqttUser", cfg.mqttUser);
  prefs.putString("mqttPass", cfg.mqttPass);
  prefs.putInt("idx", cfg.domoticzIdx);
  prefs.putInt("schemeIdx", cfg.domoticzSchemeIdx);

  prefs.putString("otaPin", cfg.otaPin);

  prefs.putUShort("ledCount", cfg.ledCount);
  prefs.putUChar("bright", cfg.brightness);
  prefs.putUChar("sat", cfg.saturation);
  prefs.putUChar("hue", cfg.hue);
  prefs.putUChar("scheme", cfg.scheme);
  prefs.putBool("power", cfg.power);
  prefs.putUChar("speed", cfg.speed);
}
