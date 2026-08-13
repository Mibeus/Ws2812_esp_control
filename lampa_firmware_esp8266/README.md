# Lampa – firmware pre ESP8266 (náhrada za Tasmota)

Funkčne identické s ESP32-C3 verziou – rozdiely sú len v niekoľkých platformových
knižniciach (WiFi, web server, ukladanie nastavení). Ovládanie tlačidlom, web
rozhranie, MQTT/Domoticz protokol a LED efekty sú 1:1 rovnaké.

## Zapojenie
- WS2812 dátový vodič → **GPIO4** (na Wemos D1 mini označené ako `D2`)
- TTP223 výstup → **GPIO5** (na Wemos D1 mini označené ako `D1`)

## Potrebné knižnice (Arduino IDE → Library Manager)
| Knižnica | Autor |
|---|---|
| WiFiManager | tzapu |
| PubSubClient | knolleary |
| Adafruit NeoPixel | Adafruit |
| ArduinoJson | bblanchon |

`ESP8266HTTPUpdateServer` a `ESP8266mDNS` sú súčasťou ESP8266 Arduino core, netreba ich inštalovať zvlášť.

## Nastavenie dosky (Tools menu)
- Board: podľa konkrétneho modulu (napr. **"LOLIN(WEMOS) D1 R2 & mini"** alebo **"NodeMCU 1.0 (ESP-12E Module)"**)
- **Flash Size**: vyber variantu, kde je v názve **"OTA"** (napr. `4MB (FS:1MB OTA:~1019KB)`).
  Na rozdiel od ESP32 tu nie je samostatné menu "Partition Scheme" – priestor pre OTA
  sa odvodzuje priamo z vybranej Flash Size varianty.

## Prvé spustenie
1. Nahraj firmware cez USB (Arduino IDE).
2. Zariadenie vytvorí WiFi hotspot **"Lampa-Setup"**. Pripoj sa naň mobilom/PC.
3. Otvorí sa (alebo otvor `192.168.4.1`) stránka s výberom WiFi siete a poľom
   **"PIN kod pre update firmveru"** – zadaj ľubovoľný kód, bude slúžiť ako heslo
   pri nahrávaní nového `.bin` cez web.
4. Po pripojení nájdeš IP vo Serial monitore, alebo choď na **http://ws2812.local**

## Bežná prevádzka – tlačidlo
- **1× klik** – zapnúť / vypnúť
- **2× klik rýchlo** – ďalší režim svietenia
- **podržanie** – plynulá zmena jasu (smer sa strieda)
- **3× klik rýchlo** – znovu spustí WiFi config portál (zmena siete / PIN kódu)

## Pomenovanie zariadenia
Na stránke `/settings` v bloku "Pomenovanie zariadenia" môžeš lampu premenovať
(napr. "Lampa syn"). Ovplyvní to nadpis na stránkach aj `.local` adresu a názov
AP hotspotu pri nastavovaní WiFi. Zmena .local adresy sa prejaví až po reštarte.

## Web rozhranie
- `/` – zapnutie/vypnutie, režim, jas, sýtosť, odtieň, rýchlosť efektov, počet LED
- `/settings` – MQTT, Domoticz, pomenovanie zariadenia, zmena WiFi
- `/update` – nahratie `.bin` (rieši to vstavaná `ESP8266HTTPUpdateServer`, chránené menom
  `admin` a PIN kódom z WiFi nastavenia)

## Domoticz / MQTT

### 1. Zariadenie pre On/Off + RGB farbu
Vo Domoticz: Setup -> Hardware -> tvoj Dummy hardware -> Create Virtual Sensors ->
typ **Color Switch** (RGBW). Zisti jeho IDx (Setup -> Devices) a zadaj do polia
"IDx svetla" na stranke /settings.

### 2. Zariadenie pre vyber rezimu
Vytvor dalsi virtualny senzor, typ **Selector Switch**. V Edit tohto zariadenia
nastav "Level names" presne v tomto poradi (oddelene znakom |):

    Pevna farba|Wakeup|Cyklus hore|Cyklus dole|Nahodne|Sviecka|RGB vzor|Duha

Zisti jeho IDx a zadaj do polia "IDx pre vyber rezimu" na stranke /settings.

### Ako to funguje
Firmware odobera topic domoticz/out a rozlisuje spravy podla idx:
- IDx Color Switch: cita Color objekt (r/g/b), Level (jas 0-100%) a nvalue (on/off)
- IDx Selector Switch: cita nvalue v nasobkoch 10 (0=prva uroven, 10=druha, ...) a mapuje na rezim

Pri zmene farby/jasu/rezimu z webu alebo tlacidla lampa publikuje spat na
topic domoticz/in, takze aj Domoticz UI zostava synchronizovane.

## Rezimy svietenia
Rovnaké ako v ESP32 verzii – 0 (pevná farba), 1 (wakeup), 2/3 (cyklus hore/dole),
4 (náhodné farby), 6 (sviečka), 7 (RGB vzor), 11 (dúha).

## Dôležitý rozdiel oproti ESP32 verzii – EEPROM namiesto NVS
Nastavenia sa neukladajú do NVS (Preferences), ale do emulovanej EEPROM (`config.cpp`).
Funkčne je to rovnaké (prežije výpadok napájania), len technicky iný mechanizmus – ESP8266
Preferences kniznicu nema.

## Známe obmedzenia
- Wakeup efekt beží fixne ~60s (konštanta v `led_control.cpp`, nezmenené oproti ESP32 verzii).
- Zmena počtu LED v `/` vyžaduje reštart (alokácia NeoPixel objektu je len pri štarte).
- ESP8266 má menej RAM než ESP32-C3 – pri výrazne väčšom počte LED (stovky) alebo pri
  súčasnom veľkom WiFiManager portáli sleduj voľnú pamäť (Serial.println(ESP.getFreeHeap())),
  pri 30 LED to však nebude problém.
