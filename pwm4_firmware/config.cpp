#include "config.h"
#include <Preferences.h>

Config cfg;
static Preferences prefs;

void configBegin() {
  prefs.begin("pwmcfg", false);
  configLoad();
}

void configLoad() {
  String s;

  s = prefs.getString("devName", "Svetla");
  s.toCharArray(cfg.deviceName, sizeof(cfg.deviceName));

  s = prefs.getString("mqttHost", "");
  s.toCharArray(cfg.mqttHost, sizeof(cfg.mqttHost));
  cfg.mqttPort = prefs.getUShort("mqttPort", 1883);
  s = prefs.getString("mqttUser", "");
  s.toCharArray(cfg.mqttUser, sizeof(cfg.mqttUser));
  s = prefs.getString("mqttPass", "");
  s.toCharArray(cfg.mqttPass, sizeof(cfg.mqttPass));

  s = prefs.getString("otaPin", "1234");
  s.toCharArray(cfg.otaPin, sizeof(cfg.otaPin));

  for (int i = 0; i < NUM_CHANNELS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "idx%d", i);
    cfg.domoticzIdx[i] = prefs.getInt(key, 0);

    snprintf(key, sizeof(key), "pwr%d", i);
    cfg.power[i] = prefs.getBool(key, true);

    snprintf(key, sizeof(key), "br%d", i);
    cfg.brightness[i] = prefs.getUChar(key, 128);

    snprintf(key, sizeof(key), "sch%d", i);
    cfg.scheme[i] = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "spd%d", i);
    cfg.speed[i] = prefs.getUChar(key, 8);
  }
}

void configSave() {
  prefs.putString("devName", cfg.deviceName);

  prefs.putString("mqttHost", cfg.mqttHost);
  prefs.putUShort("mqttPort", cfg.mqttPort);
  prefs.putString("mqttUser", cfg.mqttUser);
  prefs.putString("mqttPass", cfg.mqttPass);

  prefs.putString("otaPin", cfg.otaPin);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    char key[16];
    snprintf(key, sizeof(key), "idx%d", i);
    prefs.putInt(key, cfg.domoticzIdx[i]);

    snprintf(key, sizeof(key), "pwr%d", i);
    prefs.putBool(key, cfg.power[i]);

    snprintf(key, sizeof(key), "br%d", i);
    prefs.putUChar(key, cfg.brightness[i]);

    snprintf(key, sizeof(key), "sch%d", i);
    prefs.putUChar(key, cfg.scheme[i]);

    snprintf(key, sizeof(key), "spd%d", i);
    prefs.putUChar(key, cfg.speed[i]);
  }
}
