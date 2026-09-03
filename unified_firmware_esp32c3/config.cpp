#include "config.h"
#include <Preferences.h>

Config cfg;
static Preferences prefs;

void configBegin() {
  prefs.begin("cfg", false);
  configLoad();
}

void configLoad() {
  String s;

  s = prefs.getString("devName", "Zariadenie");
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

  for (int i = 0; i < NUM_SLOTS; i++) {
    char key[16];
    SlotConfig &sl = cfg.slots[i];

    snprintf(key, sizeof(key), "s%dgpio", i);  sl.gpio = prefs.getUChar(key, i);
    snprintf(key, sizeof(key), "s%dfunc", i);  sl.function = prefs.getUChar(key, FUNC_NONE);
    snprintf(key, sizeof(key), "s%dpwr", i);   sl.power = prefs.getBool(key, true);
    snprintf(key, sizeof(key), "s%dbr", i);    sl.brightness = prefs.getUChar(key, 128);
    snprintf(key, sizeof(key), "s%dhue", i);   sl.hue = prefs.getUChar(key, 0);
    snprintf(key, sizeof(key), "s%dsat", i);   sl.saturation = prefs.getUChar(key, 255);
    snprintf(key, sizeof(key), "s%dsch", i);   sl.scheme = prefs.getUChar(key, 0);
    snprintf(key, sizeof(key), "s%dspd", i);   sl.speed = prefs.getUChar(key, 8);
    snprintf(key, sizeof(key), "s%dlc", i);    sl.ledCount = prefs.getUShort(key, 30);
    snprintf(key, sizeof(key), "s%didx", i);   sl.domoticzIdx = prefs.getInt(key, 0);
    snprintf(key, sizeof(key), "s%dsidx", i);  sl.domoticzSchemeIdx = prefs.getInt(key, 0);
  }
}

void configSave() {
  prefs.putString("devName", cfg.deviceName);

  prefs.putString("mqttHost", cfg.mqttHost);
  prefs.putUShort("mqttPort", cfg.mqttPort);
  prefs.putString("mqttUser", cfg.mqttUser);
  prefs.putString("mqttPass", cfg.mqttPass);

  prefs.putString("otaPin", cfg.otaPin);

  for (int i = 0; i < NUM_SLOTS; i++) {
    char key[16];
    SlotConfig &sl = cfg.slots[i];

    snprintf(key, sizeof(key), "s%dgpio", i);  prefs.putUChar(key, sl.gpio);
    snprintf(key, sizeof(key), "s%dfunc", i);  prefs.putUChar(key, sl.function);
    snprintf(key, sizeof(key), "s%dpwr", i);   prefs.putBool(key, sl.power);
    snprintf(key, sizeof(key), "s%dbr", i);    prefs.putUChar(key, sl.brightness);
    snprintf(key, sizeof(key), "s%dhue", i);   prefs.putUChar(key, sl.hue);
    snprintf(key, sizeof(key), "s%dsat", i);   prefs.putUChar(key, sl.saturation);
    snprintf(key, sizeof(key), "s%dsch", i);   prefs.putUChar(key, sl.scheme);
    snprintf(key, sizeof(key), "s%dspd", i);   prefs.putUChar(key, sl.speed);
    snprintf(key, sizeof(key), "s%dlc", i);    prefs.putUShort(key, sl.ledCount);
    snprintf(key, sizeof(key), "s%didx", i);   prefs.putInt(key, sl.domoticzIdx);
    snprintf(key, sizeof(key), "s%dsidx", i);  prefs.putInt(key, sl.domoticzSchemeIdx);
  }
}
