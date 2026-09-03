# Zjednoteny firmware (ESP8266)

Kombinuje funkcionalitu vsetkych predoslych projektov (WS2812 lampa, PWM svetla)
do jedneho firmveru. Kazdy zo 4 pinov ma v `/settings` nastavitelnu:
- **GPIO cislo** (kam je fyzicky pripojeny)
- **Funkciu**: Nevyuzite / Status LED / PWM svetlo / WS2812 pas / On-Off

Vdaka tomu jeden firmware pokryje viacero vasich dosiek bez potreby prekompilovat
kod - staci prekonfigurovat piny vo webovom rozhrani (vyzaduje restart).

## Funkcie pinov

| Funkcia | Popis | Domoticz typ |
|---|---|---|
| Status LED | automaticka signalizacia WiFi/MQTT (bez WiFi=zablesk, WiFi bez MQTT=blika 1:1, MQTT=trvalo svieti) | - |
| PWM svetlo | 1 nezavisly kanal, len jas, 5 rezimov (staticka/wakeup/pulzovanie/nahodne/sviecka) | Dimmer |
| WS2812 pas | adresovatelny LED pas, farba+jas, 8 rezimov (rovnake ako povodna lampa) | Color Switch |
| On/Off | jednoduchy digitalny vystup (napr. rele) | Switch |

Kazdy PWM a WS2812 slot moze mat aj volitelny druhy Domoticz IDx (Selector Switch)
pre vyber rezimu priamo z Domoticz UI.

## Kniznice
WiFiManager (tzapu), PubSubClient (knolleary), Adafruit NeoPixel, ArduinoJson (bblanchon).

## Diagnostika
Stranka `/debug` ukazuje pre kazdy pin jeho funkciu, GPIO, (pri PWM) uspesnost
inicializacie a aktualne hodnoty - bez potreby Serial monitora.

## OTA aktualizacia
Rovnaky flow ako v predoslych projektoch - `/update` s PIN kodom v tom istom
formulari ako vyber suboru (ziadne prehliadacove prihlasovacie okno).
