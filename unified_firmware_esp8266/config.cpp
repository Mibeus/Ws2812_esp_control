#include "config.h"
#include <EEPROM.h>

Config cfg;

static const uint32_t CFG_MAGIC = 0x554E4931; // "UNI1"

struct StoredConfig {
  uint32_t magic;
  Config data;
};

static const size_t EEPROM_SIZE = sizeof(StoredConfig) + 16;

void configBegin() {
  EEPROM.begin(EEPROM_SIZE);
  configLoad();
}

void configLoad() {
  StoredConfig sc;
  EEPROM.get(0, sc);
  if (sc.magic == CFG_MAGIC) {
    cfg = sc.data;
  } else {
    cfg = Config(); // prvy start / prazdna EEPROM -> vychodzie hodnoty
  }
}

void configSave() {
  StoredConfig sc;
  sc.magic = CFG_MAGIC;
  sc.data = cfg;
  EEPROM.put(0, sc);
  EEPROM.commit();
}
