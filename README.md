# Lampa firmware – automaticka kompilacia cez GitHub Actions

Tento repozitar obsahuje oba varianty firmveru (ESP32-C3 aj ESP8266) a workflow,
ktory ich pri kazdom push-i automaticky skompiluje a vysledny `.bin` prilozi
ako GitHub Release. Vdaka tomu netreba mat lokalne nainstalovane Arduino IDE
ani ESP32/ESP8266 toolchain – kompiluje to GitHub, nie tvoj pocitac.

## Struktura
```
lampa_firmware/            <- ESP32-C3 sketch
lampa_firmware_esp8266/    <- ESP8266 sketch
.github/workflows/build.yml
```

## Prve nastavenie (jednorazovo)

1. Na GitHub vytvor novy **repozitar** (moze byt verejny - vdaka tomu viem
   sam stiahnut hotovy .bin bez potreby prihlasovacich udajov).
2. Nahraj do neho tento cely obsah:
   - **Bez gitu**: na stranke repozitara klikni "Add file" -> "Upload files"
     a pretiahni tam vsetky priecinky/subory (aj skryty `.github` priecinok -
     ten sa cez web rozhranie musi nahrat aj s cestou, GitHub si ho spravne
     zaradi).
   - **S gitom**:
     ```
     git init
     git add .
     git commit -m "Prvotny import"
     git branch -M main
     git remote add origin https://github.com/<tvoj-ucet>/<nazov-repa>.git
     git push -u origin main
     ```
3. Po push-i sa v zalozke **Actions** automaticky spusti workflow "Build
   firmware" (trva cca 3-5 min - stahuje a instaluje cele ESP32/ESP8266
   jadra). Po dobehnuti najdes v zalozke **Releases** novy zaznam s dvoma
   prilohami: `lampa_firmware_esp32c3.bin` a `lampa_firmware_esp8266.bin`.

## Dalsie buildy
- Kazdy dalsi push do vetvy `main` spusti novy build automaticky.
- Alebo v zalozke Actions -> "Build firmware" -> "Run workflow" (tlacidlo
  vpravo hore) spustis build rucne aj bez zmeny kodu.

## Ako z toho dostanem .bin ja (Claude)
Ked mi napises, ze si repozitar vytvoril / pushol zmenu, viem cez GitHub API
(`api.github.com`) zistit najnovsi Release a stiahnut prilohy priamo z
`release-assets.githubusercontent.com` - tieto domeny mam v mojom prostredi
povolene. Staci mi poslat **odkaz na repozitar** (napr.
`https://github.com/<ucet>/<repo>`), zvysok si viem dohladat sam.
