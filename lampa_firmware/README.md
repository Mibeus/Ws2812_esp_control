# Lampa – vlastný firmware (ESP32-C3, náhrada za Tasmota)

## Zapojenie
- WS2812 dátový vodič → **GPIO4**
- TTP223 výstup → **GPIO5** (predpoklad: modul je v "momentary/direct" móde z výroby –
  výstup HIGH počas dotyku. Ak máš spájkovaný jumper na "toggle" mód, uprav logiku v `button.cpp`.)

## Potrebné knižnice (Arduino IDE → Library Manager)
| Knižnica | Autor |
|---|---|
| WiFiManager | tzapu |
| PubSubClient | knolleary |
| Adafruit NeoPixel | Adafruit |
| ArduinoJson | bblanchon |

## Nastavenie dosky (Tools menu)
- Board: **ESP32C3 Dev Module**
- Partition Scheme: **Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)**
  (nutné pre 4 MB flash + dve OTA app partície + web súbory)
- Upload Speed: pokojne 921600, USB CDC On Boot: Enabled (kvôli Serial cez USB na super mini doske)

## Prvé spustenie
1. Nahraj firmware cez USB (Arduino IDE).
2. Zariadenie vytvorí WiFi hotspot **"Lampa-Setup"**. Pripoj sa naň mobilom/PC.
3. Otvorí sa (alebo otvor v prehliadači `192.168.4.1`) stránka s výberom WiFi siete
   a poľom **"PIN kod pre update firmveru"** – zadaj ľubovoľný kód, bude slúžiť
   ako heslo pri nahrávaní nového `.bin` súboru cez web.
4. Po úspešnom pripojení nájdeš IP adresu vo Serial monitore, alebo choď priamo na
   **http://ws2812.local**

## Bežná prevádzka – ovládanie tlačidlom
- **1× klik** – zapnúť / vypnúť
- **2× klik rýchlo po sebe** – ďalší režim svietenia
- **podržanie** – plynulá zmena jasu (smer sa strieda pri každom ďalšom podržaní)
- **3× klik rýchlo po sebe** – znovu spustí WiFi config portál (zmena siete / PIN kódu)

## Pomenovanie zariadenia
Na stránke `/settings` v bloku "Pomenovanie zariadenia" môžeš lampu premenovať
(napr. "Lampa syn"). Ovplyvní to nadpis na stránkach aj `.local` adresu a názov
AP hotspotu pri nastavovaní WiFi. Zmena .local adresy sa prejaví až po reštarte.

## Web rozhranie
- `/` – zapnutie/vypnutie, výber režimu, jas, sýtosť farby, odtieň, rýchlosť efektov, počet LED
- `/settings` – MQTT, Domoticz, pomenovanie zariadenia, zmena WiFi
- `/update` – nahratie nového `.bin` (chránené menom `admin` a PIN kódom z WiFi nastavenia)

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

## Rezimy svietenia (čísla zhodné s Tasmota "Scheme")
| Scheme | Popis |
|---|---|
| 0 | Pevná farba |
| 1 | Wakeup – postupný nábeh jasu (~60s) |
| 2 | Cyklus farieb hore |
| 3 | Cyklus farieb dole |
| 4 | Náhodné farby s prechodom |
| 6 | Sviečka (flicker, teplá oranžová) |
| 7 | RGB vzor (striedanie blokov) |
| 11 | Dúha (pohybujúci sa gradient) |

## Známe obmedzenia / veci na doladenie
- Wakeup efekt beží fixne ~60s, dá sa upraviť v `led_control.cpp` (konštanta v `renderWakeup()`).
- Rýchlosť animácií (1–20) je spoločná pre všetky efekty naraz, nie per-efekt.
- Ak zmeníš počet LED v `/`, potrebný je reštart zariadenia (alokácia NeoPixel objektu je len pri štarte).
