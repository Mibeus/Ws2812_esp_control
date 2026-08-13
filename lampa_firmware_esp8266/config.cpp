#include "config.h"
#include <EEPROM.h>

Config cfg;

// Na ESP8266 neexistuje Preferences/NVS ako na ESP32, pouzivame emulovanu EEPROM.
// Cely (POD) Config struct sa ulozi/nacita naraz cez EEPROM.put/get.
static const uint32_t CFG_MAGIC = 0x4C4D5031; // "LMP1" - oznacuje platne ulozene data

struct StoredConfig {
  uint32_t magic;
  Config data;
};

static const size_t EEPROM_SIZE = sizeof(StoredConfig) + 8;

void configBegin() {
  EEPROM.begin(EEPROM_SIZE);
  configLoad();
}

void configLoad() {
  StoredConfig sc;
  EEPROM.get(0, sc);
  if (sc.magic == CFG_MAGIC) {
    cfg = sc.data;       // najdene platne ulozene nastavenia
  } else {
    cfg = Config();      // prvy start / prazdna EEPROM -> pouzi vychodzie hodnoty
  }
}

void configSave() {
  StoredConfig sc;
  sc.magic = CFG_MAGIC;
  sc.data = cfg;
  EEPROM.put(0, sc);
  EEPROM.commit();  // na ESP8266 je commit() nutny, inak sa zapis neulozi do flash
}
