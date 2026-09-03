# Svetla – 4 nezavisle PWM kanaly (ESP32-C3, 4MB flash)

Samostatny projekt odvodeny od WS2812 lampy - rovnaky vizual, rovnaky spôsob
ovladania (web/MQTT/Domoticz/OTA), ale namiesto WS2812 pasu ovlada **4 uplne
nezavisle PWM svetla** (bez farby, len jas).

## Zapojenie
- Kanal 1 -> GPIO0
- Kanal 2 -> GPIO1
- Kanal 3 -> GPIO2
- Kanal 4 -> GPIO3

## Diagnostika
Kym nefunguje Serial monitor, stav kazdeho kanala (ledcAttach uspech, cfg hodnoty,
naposledy zapisana PWM hodnota) je dostupny priamo na stranke `/debug`.

## Rezimy svietenia (kazdy kanal nezavisle)
| Cislo | Nazov | Popis |
|---|---|---|
| 0 | Staticka | pevny jas |
| 1 | Wakeup | plynuly nabeh pri zapnuti / plynule zhasnutie pri vypnuti |
| 2 | Pulzovanie | opakovane plynule stmievanie hore-dole |
| 3 | Nahodne | nahodne plynule zmeny jasu |
| 4 | Sviecka | blikajuci plamen |

Ziadne farebne efekty (RGB vzor, dúha, cyklus farieb) - tie boli specificke
pre WS2812 a bez farby nedavaju zmysel.

## Ovladanie
Ziadne fyzicke tlacidlo - len web rozhranie, MQTT a Domoticz.

## Domoticz
Kazdy kanal = samostatne virtualne zariadenie typu **Dimmer**. V `/settings`
zadaj IDx pre kazdy zo 4 kanalov zvlast. Protokol:
- prichadzajuce: `{"idx":X,"nvalue":0/1,"svalue1":"<0-100%>"}`
- odchadzajuce: `{"idx":X,"nvalue":0/1,"svalue":"<0-100%>"}`

## Kniznice
WiFiManager (tzapu), PubSubClient (knolleary), ArduinoJson (bblanchon).

## OTA aktualizacia
Rovnaky flow ako pri WS2812 lampe - `/update` stranka s PIN kodom (nastavuje
sa spolu s WiFi). 4MB flash bohato stacia na OTA (dve app particie).
