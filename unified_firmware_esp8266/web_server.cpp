#include "web_server.h"
#include "config.h"
#include "pins.h"
#include "effects_pwm.h"
#include "effects_ws2812.h"
#include "mqtt_client.h"
#include "wifi_setup.h"
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ESP8266WiFi.h>

static ESP8266WebServer server(80);
static bool otaPinOk = false;

// ---------------------------------------------------------------------
// Spolocny vizualny styl (rovnaky ako v predchadzajucich projektoch)
// ---------------------------------------------------------------------
static const char PAGE_STYLE[] PROGMEM = R"CSS(
:root{
  --accent:#00e5ff; --accent2:#ff9d00; --bg:#050a12; --panel:#0b1622;
  --panel-border:#123047; --text:#d8f3ff; --text-dim:#5c7c8f;
}
*{box-sizing:border-box;}
body{
  margin:0; padding:24px 14px 60px; min-height:100vh;
  background:radial-gradient(circle at 50% -10%, #0d2436 0%, var(--bg) 55%);
  font-family:'Consolas','Segoe UI',system-ui,monospace; color:var(--text);
}
.wrap{max-width:460px;margin:0 auto;}
h1{font-size:20px;letter-spacing:4px;text-transform:uppercase;text-align:center;
  margin:0 0 4px;color:var(--accent);text-shadow:0 0 12px rgba(0,229,255,.6);}
.subtitle{text-align:center;color:var(--text-dim);font-size:11px;letter-spacing:2px;
  margin-bottom:22px;text-transform:uppercase;}
h2.chTitle{font-size:14px;letter-spacing:2px;text-transform:uppercase;color:var(--accent);margin:0;}
.panel{background:linear-gradient(180deg,var(--panel),#081019);border:1px solid var(--panel-border);
  border-radius:14px;padding:18px 18px 22px;margin-bottom:16px;
  box-shadow:0 0 0 1px rgba(0,229,255,.05),0 0 25px rgba(0,229,255,.08),inset 0 0 30px rgba(0,229,255,.03);
  position:relative;}
.panel::before{content:'';position:absolute;top:0;left:18px;right:18px;height:1px;
  background:linear-gradient(90deg,transparent,var(--accent),transparent);opacity:.5;}
.row{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;}
.row:last-child{margin-bottom:0;}
.status-line{display:flex;justify-content:space-between;font-size:11px;color:var(--text-dim);
  letter-spacing:1px;margin-bottom:18px;text-transform:uppercase;}
.status-line b{color:var(--accent);font-weight:normal;}
label.field{display:block;font-size:11px;letter-spacing:2px;text-transform:uppercase;
  color:var(--text-dim);margin:14px 0 6px;}
label.field:first-child{margin-top:0;}
select,input[type=number],input[type=password],input[type=text]{
  width:100%;padding:10px;background:#081420;color:var(--text);
  border:1px solid var(--panel-border);border-radius:8px;font-family:inherit;font-size:14px;}
input[type=file]{width:100%;padding:8px;background:#081420;color:var(--text-dim);
  border:1px dashed var(--panel-border);border-radius:8px;font-family:inherit;font-size:13px;}
input[type=range]{-webkit-appearance:none;width:100%;height:6px;border-radius:4px;
  background:linear-gradient(90deg,#0c2130,var(--accent));outline:none;}
input[type=range].hueSl{background:linear-gradient(90deg,red,yellow,lime,cyan,blue,magenta,red);}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
.colorPreview{width:100%;height:44px;border-radius:10px;margin-top:8px;border:1px solid var(--panel-border);
  transition:background .08s linear, box-shadow .08s linear;}
.switch{position:relative;display:inline-block;width:56px;height:30px;}
.switch input{opacity:0;width:0;height:0;}
.slider-toggle{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#0c2130;
  border:1px solid var(--panel-border);border-radius:30px;transition:.2s;}
.slider-toggle:before{content:'';position:absolute;height:22px;width:22px;left:3px;bottom:3px;
  background:#3a5468;border-radius:50%;transition:.2s;}
.switch input:checked + .slider-toggle{background:rgba(0,229,255,.15);border-color:var(--accent);
  box-shadow:0 0 14px rgba(0,229,255,.5);}
.switch input:checked + .slider-toggle:before{transform:translateX(26px);background:var(--accent);
  box-shadow:0 0 8px var(--accent);}
.value-tag{float:right;color:var(--accent);font-size:11px;}
a.navlink{display:block;text-align:center;padding:12px;margin-top:6px;color:var(--accent);
  text-decoration:none;font-size:12px;letter-spacing:2px;text-transform:uppercase;
  border:1px solid var(--panel-border);border-radius:10px;}
a.navlink:active{background:rgba(0,229,255,.08);}
button.primary{width:100%;padding:12px;margin-top:16px;border:none;border-radius:8px;
  background:linear-gradient(180deg,var(--accent2),#c76e00);color:#101010;font-weight:bold;
  letter-spacing:1px;text-transform:uppercase;font-family:inherit;cursor:pointer;}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px;}
.dot.on{background:#33ff9d;box-shadow:0 0 8px #33ff9d;}
.dot.off{background:#555;}
.hint{color:var(--text-dim);font-size:12px;line-height:1.5;margin:0 0 6px;}
.funcTag{font-size:10px;color:var(--text-dim);letter-spacing:1px;text-transform:uppercase;}
)CSS";

static String pageOpen(const String &title) {
  String s;
  s.reserve(3200);
  s += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<title>" + title + "</title><style>";
  s += FPSTR(PAGE_STYLE);
  s += "</style></head><body><div class='wrap'>";
  return s;
}
static const char PAGE_CLOSE[] = "</div></body></html>";

static const char* WS_SCHEME_NAMES[] = {
  "0 - Pevna farba", "1 - Wakeup (nabeh/zhasnutie)", "2 - Cyklus farieb hore",
  "3 - Cyklus farieb dole", "4 - Nahodne farby", "6 - Sviecka", "7 - RGB vzor", "11 - Duha"
};
static const uint8_t WS_SCHEME_VALS[] = {0, 1, 2, 3, 4, 6, 7, 11};

static const char* PWM_SCHEME_NAMES[] = {
  "0 - Staticka", "1 - Wakeup (nabeh/zhasnutie)", "2 - Pulzovanie", "3 - Nahodne", "4 - Sviecka"
};

static const char* FUNC_NAMES[FUNC_COUNT] = {
  "Nevyuzite", "Status LED", "PWM svetlo", "WS2812 pas", "On/Off"
};

// Odporucane/bezpecne GPIO na ESP32-C3 - vynechane strapping piny (9),
// nativny USB (18,19) a UART0 (20,21, kvoli spolahlivemu Serial monitoru).
static const uint8_t ALLOWED_GPIOS[] = {4, 5, 12, 13, 14}; // D2,D1,D6,D7,D5 - najbezpecnejsie na Wemos D1 mini style doskach
static const uint8_t ALLOWED_GPIO_COUNT = sizeof(ALLOWED_GPIOS) / sizeof(ALLOWED_GPIOS[0]);

// ---------------------------------------------------------------------
// "/" - hlavna stranka, panel pre kazdy aktivny slot (podla funkcie)
// ---------------------------------------------------------------------
static void appendOnOffPanel(String &s, uint8_t ch) {
  SlotConfig &sl = cfg.slots[ch];
  s += "<div class='panel'><div class='row'><div><h2 class='chTitle'>Kanal " + String(ch + 1) + "</h2>";
  s += "<span class='funcTag'>On/Off - GPIO" + String(sl.gpio) + "</span></div>";
  s += "<label class='switch'><input type='checkbox' class='pwrToggle' data-ch='" + String(ch) + "'";
  s += (sl.power ? " checked" : "");
  s += "><span class='slider-toggle'></span></label></div></div>";
}

static void appendStatusLedPanel(String &s, uint8_t ch) {
  SlotConfig &sl = cfg.slots[ch];
  s += "<div class='panel'><h2 class='chTitle'>Kanal " + String(ch + 1) + "</h2>";
  s += "<span class='funcTag'>Status LED - GPIO" + String(sl.gpio) + "</span>";
  s += "<p class='hint'>Automaticka signalizacia WiFi/MQTT stavu, bez rucneho ovladania.</p></div>";
}

static void appendPwmPanel(String &s, uint8_t ch) {
  SlotConfig &sl = cfg.slots[ch];
  s += "<div class='panel'>";
  s += "<div class='row'><div><h2 class='chTitle'>Kanal " + String(ch + 1) + "</h2>";
  s += "<span class='funcTag'>PWM - GPIO" + String(sl.gpio) + "</span></div>";
  s += "<label class='switch'><input type='checkbox' class='pwrToggle' data-ch='" + String(ch) + "'";
  s += (sl.power ? " checked" : "");
  s += "><span class='slider-toggle'></span></label></div>";

  s += "<label class='field'>Rezim</label><select class='schemeSel' data-ch='" + String(ch) + "'>";
  for (uint8_t i = 0; i < PWM_SCHEME_COUNT; i++) {
    s += "<option value='" + String(i) + "'" + (sl.scheme == i ? " selected" : "") + ">" + PWM_SCHEME_NAMES[i] + "</option>";
  }
  s += "</select>";

  s += "<label class='field'>Jas <span class='value-tag brightVal' data-ch='" + String(ch) + "'>" + String(sl.brightness) + "</span></label>";
  s += "<input type='range' min='0' max='255' class='brightSl' data-ch='" + String(ch) + "' value='" + String(sl.brightness) + "'>";

  s += "<label class='field'>Rychlost efektu <span class='value-tag speedVal' data-ch='" + String(ch) + "'>" + String(sl.speed) + "</span></label>";
  s += "<input type='range' min='1' max='20' class='speedSl' data-ch='" + String(ch) + "' value='" + String(sl.speed) + "'>";
  s += "</div>";
}

static void appendWs2812Panel(String &s, uint8_t ch) {
  SlotConfig &sl = cfg.slots[ch];
  s += "<div class='panel'>";
  s += "<div class='row'><div><h2 class='chTitle'>Kanal " + String(ch + 1) + "</h2>";
  s += "<span class='funcTag'>WS2812 - GPIO" + String(sl.gpio) + "</span></div>";
  s += "<label class='switch'><input type='checkbox' class='pwrToggle' data-ch='" + String(ch) + "'";
  s += (sl.power ? " checked" : "");
  s += "><span class='slider-toggle'></span></label></div>";

  s += "<label class='field'>Rezim</label><select class='schemeSel' data-ch='" + String(ch) + "'>";
  for (uint8_t i = 0; i < 8; i++) {
    s += "<option value='" + String(WS_SCHEME_VALS[i]) + "'" + (sl.scheme == WS_SCHEME_VALS[i] ? " selected" : "") + ">" + WS_SCHEME_NAMES[i] + "</option>";
  }
  s += "</select>";

  s += "<label class='field'>Jas <span class='value-tag brightVal' data-ch='" + String(ch) + "'>" + String(sl.brightness) + "</span></label>";
  s += "<input type='range' min='0' max='255' class='brightSl' data-ch='" + String(ch) + "' value='" + String(sl.brightness) + "'>";

  s += "<label class='field'>Sytost farby <span class='value-tag satVal' data-ch='" + String(ch) + "'>" + String(sl.saturation) + "</span></label>";
  s += "<input type='range' min='0' max='255' class='satSl' data-ch='" + String(ch) + "' value='" + String(sl.saturation) + "'>";

  s += "<label class='field'>Odtien farby <span class='value-tag hueVal' data-ch='" + String(ch) + "'>" + String(sl.hue) + "</span></label>";
  s += "<input type='range' min='0' max='255' class='hueSl' data-ch='" + String(ch) + "' value='" + String(sl.hue) + "'>";
  s += "<div class='colorPreview' id='colorPreview" + String(ch) + "'></div>";

  s += "<label class='field'>Rychlost efektu <span class='value-tag speedVal' data-ch='" + String(ch) + "'>" + String(sl.speed) + "</span></label>";
  s += "<input type='range' min='1' max='20' class='speedSl' data-ch='" + String(ch) + "' value='" + String(sl.speed) + "'>";
  s += "</div>";
}

static void handleRoot() {
  String s = pageOpen(String(cfg.deviceName) + " // Ovladaci panel");
  s.reserve(s.length() + 7000);
  s += "<h1>&#9670; " + String(cfg.deviceName) + "</h1><div class='subtitle'>Ovladaci panel</div>";

  if (pinsIsSafeMode()) {
    s += "<div class='panel' style='border-color:#ff4d4d'><p class='hint' style='color:#ff9d9d'>";
    s += "&#9888; SAFE MODE: zariadenie opakovane zlyhalo hned po starte, piny su docasne vypnute. ";
    s += "Choď do Nastaveni, over/oprav konfiguraciu pinov a ulož + restartuj.</p></div>";
  }

  s += "<div class='status-line'><span><span class='dot ";
  s += (mqttIsConnected() ? "on" : "off");
  s += "'></span>MQTT: <b>" + String(mqttIsConnected() ? "pripojene" : "-") + "</b></span>";
  s += "<span>IP: <b>" + wifiGetIp() + "</b></span></div>";

  bool anyActive = false;
  for (uint8_t ch = 0; ch < NUM_SLOTS; ch++) {
    switch (cfg.slots[ch].function) {
      case FUNC_ONOFF:      appendOnOffPanel(s, ch);    anyActive = true; break;
      case FUNC_STATUS_LED: appendStatusLedPanel(s, ch); anyActive = true; break;
      case FUNC_PWM:        appendPwmPanel(s, ch);      anyActive = true; break;
      case FUNC_WS2812:     appendWs2812Panel(s, ch);   anyActive = true; break;
      default: break;
    }
  }
  if (!anyActive) {
    s += "<div class='panel'><p class='hint'>Ziadny pin zatial nema priradenu funkciu. Choď do Nastaveni a nastav aspon jeden.</p></div>";
  }

  s += "<a class='navlink' href='/settings'>&#9881; Nastavenia</a>";
  s += "<a class='navlink' href='/debug'>&#128269; Diagnostika</a>";
  s += "<a class='navlink' href='/update'>&#8657; Aktualizacia firmveru</a>";

  s += R"JS(<script>
const $$ = sel => document.querySelectorAll(sel);
const $ = id => document.getElementById(id);

function hsv2rgb(h,s,v){h/=255;s/=255;v/=255;var i=Math.floor(h*6),f=h*6-i;
var p=v*(1-s),q=v*(1-f*s),t=v*(1-(1-f)*s),r,g,b;
switch(i%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;
case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;case 5:r=v;g=p;b=q;break;}
return [Math.round(r*255),Math.round(g*255),Math.round(b*255)];}

function updatePreview(ch){
  var hueEl=document.querySelector(".hueSl[data-ch='"+ch+"']");
  var satEl=document.querySelector(".satSl[data-ch='"+ch+"']");
  var brEl=document.querySelector(".brightSl[data-ch='"+ch+"']");
  var prev=$('colorPreview'+ch);
  if(!hueEl||!satEl||!brEl||!prev) return;
  var rgb=hsv2rgb(+hueEl.value,+satEl.value,Math.max(+brEl.value,40));
  var css='rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+')';
  prev.style.background=css;
  prev.style.boxShadow='0 0 22px '+css;
}

let liveTimer=null;
function sendLive(ch, field, value){
  clearTimeout(liveTimer);
  liveTimer=setTimeout(function(){
    fetch('/live',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'ch='+ch+'&'+field+'='+value});
  },60);
}
function sendSet(ch, body){
  fetch('/set',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ch='+ch+'&'+body});
}

$$('.pwrToggle').forEach(function(el){
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'power='+(e.target.checked?1:0)); });
});
$$('.schemeSel').forEach(function(el){
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'scheme='+e.target.value); });
});
$$('.brightSl').forEach(function(el){
  el.addEventListener('input', function(e){
    var v=document.querySelector(".brightVal[data-ch='"+e.target.dataset.ch+"']");
    if(v) v.textContent=e.target.value;
    updatePreview(e.target.dataset.ch);
    sendLive(e.target.dataset.ch, 'bright', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'bright='+e.target.value); });
});
$$('.hueSl').forEach(function(el){
  el.addEventListener('input', function(e){
    var v=document.querySelector(".hueVal[data-ch='"+e.target.dataset.ch+"']");
    if(v) v.textContent=e.target.value;
    updatePreview(e.target.dataset.ch);
    sendLive(e.target.dataset.ch, 'hue', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'hue='+e.target.value); });
});
$$('.satSl').forEach(function(el){
  el.addEventListener('input', function(e){
    var v=document.querySelector(".satVal[data-ch='"+e.target.dataset.ch+"']");
    if(v) v.textContent=e.target.value;
    updatePreview(e.target.dataset.ch);
    sendLive(e.target.dataset.ch, 'sat', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'sat='+e.target.value); });
});
$$('.speedSl').forEach(function(el){
  el.addEventListener('input', function(e){
    var v=document.querySelector(".speedVal[data-ch='"+e.target.dataset.ch+"']");
    if(v) v.textContent=e.target.value;
    sendLive(e.target.dataset.ch, 'speed', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'speed='+e.target.value); });
});

for (let i=0;i<4;i++) updatePreview(i);
</script>)JS";

  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

static int8_t argChannel() {
  if (!server.hasArg("ch")) return -1;
  int ch = server.arg("ch").toInt();
  if (ch < 0 || ch >= NUM_SLOTS) return -1;
  return (int8_t)ch;
}

static void handleLive() {
  int8_t ch = argChannel();
  if (ch < 0) { server.send(400, "text/plain", "chybny kanal"); return; }
  SlotConfig &sl = cfg.slots[ch];
  if (server.hasArg("bright")) sl.brightness = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("hue"))    sl.hue = (uint8_t)server.arg("hue").toInt();
  if (server.hasArg("sat"))    sl.saturation = (uint8_t)server.arg("sat").toInt();
  if (server.hasArg("speed"))  sl.speed = (uint8_t)server.arg("speed").toInt();
  server.send(200, "text/plain", "OK");
}

static void handleSet() {
  int8_t ch = argChannel();
  if (ch < 0) { server.send(400, "text/plain", "chybny kanal"); return; }
  SlotConfig &sl = cfg.slots[ch];

  if (server.hasArg("power"))  { sl.power = server.arg("power").toInt() != 0; pinsApplyPower(ch); }
  if (server.hasArg("scheme")) { sl.scheme = (uint8_t)server.arg("scheme").toInt(); }
  if (server.hasArg("bright")) sl.brightness = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("hue"))    sl.hue = (uint8_t)server.arg("hue").toInt();
  if (server.hasArg("sat"))    sl.saturation = (uint8_t)server.arg("sat").toInt();
  if (server.hasArg("speed"))  sl.speed = (uint8_t)server.arg("speed").toInt();

  configSave();
  mqttPublishSlot(ch);
  if (server.hasArg("scheme")) mqttPublishSlotScheme(ch);
  server.send(200, "text/plain", "OK");
}

static void handleRestart() {
  server.send(200, "text/plain", "OK");
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------
// "/settings"
// ---------------------------------------------------------------------
static void appendTableCss(String &s) {
  s += "table.pintable{width:100%;border-collapse:collapse;font-size:12px;}";
  s += "table.pintable th{text-align:left;color:var(--text-dim);font-weight:normal;text-transform:uppercase;letter-spacing:1px;font-size:10px;padding:0 4px 6px;}";
  s += "table.pintable td{padding:4px 4px 10px;vertical-align:top;}";
  s += "table.pintable select, table.pintable input{width:100%;padding:6px;font-size:12px;}";
  s += "tr.ws2812extra td{padding-top:0;}";
  s += "tr.ws2812extra label{font-size:10px;color:var(--text-dim);display:block;margin-bottom:2px;}";
  s += "tr.ws2812extra input{margin-bottom:6px;}";
}

static void handleSettingsPage() {
  String s = pageOpen("Nastavenia");
  s.reserve(s.length() + 6500);
  s += "<style>"; appendTableCss(s); s += "</style>";
  s += "<h1>&#9881; Nastavenia</h1><div class='subtitle'>" + String(cfg.deviceName) + "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>MQTT</label>";
  s += "<label class='field'>Broker (IP alebo hostname)</label><input id='host' value='" + String(cfg.mqttHost) + "'>";
  s += "<label class='field'>Port</label><input id='port' value='" + String(cfg.mqttPort) + "'>";
  s += "<label class='field'>Pouzivatelske meno (nepovinne)</label><input id='user' value='" + String(cfg.mqttUser) + "'>";
  s += "<label class='field'>Heslo (nepovinne)</label><input type='password' id='pass' value='" + String(cfg.mqttPass) + "'>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Piny</label>";
  s += "<table class='pintable'><tr><th>Pin</th><th>Funkcia</th><th>Domoticz IDx</th></tr>";

  for (uint8_t ch = 0; ch < NUM_SLOTS; ch++) {
    SlotConfig &sl = cfg.slots[ch];
    s += "<tr><td><select id='gpio" + String(ch) + "'>";
    bool gpioListed = false;
    for (uint8_t p = 0; p < ALLOWED_GPIO_COUNT; p++) {
      s += "<option value='" + String(ALLOWED_GPIOS[p]) + "'" + (sl.gpio == ALLOWED_GPIOS[p] ? " selected" : "") + ">GPIO" + String(ALLOWED_GPIOS[p]) + "</option>";
      if (sl.gpio == ALLOWED_GPIOS[p]) gpioListed = true;
    }
    if (!gpioListed) { // ulozene GPIO uz nie je v odporucanom zozname - pridaj ho, nech sa nestrati
      s += "<option value='" + String(sl.gpio) + "' selected>GPIO" + String(sl.gpio) + " (mimo zoznamu)</option>";
    }
    s += "</select></td>";

    s += "<td><select id='func" + String(ch) + "' onchange='toggleWs2812Extra(" + String(ch) + ")'>";
    for (uint8_t f = 0; f < FUNC_COUNT; f++) {
      s += "<option value='" + String(f) + "'" + (sl.function == f ? " selected" : "") + ">" + FUNC_NAMES[f] + "</option>";
    }
    s += "</select></td>";

    s += "<td><input id='idx" + String(ch) + "' value='" + String(sl.domoticzIdx) + "'></td></tr>";

    s += "<tr class='ws2812extra' id='wsrow" + String(ch) + "' style='display:" + (sl.function == FUNC_WS2812 ? "table-row" : "none") + "'>";
    s += "<td colspan='3'><label>Pocet LED</label><input type='number' min='1' max='500' id='lc" + String(ch) + "' value='" + String(sl.ledCount) + "'>";
    s += "<label>Domoticz IDx pre vyber rezimu (volitelne)</label><input id='sidx" + String(ch) + "' value='" + String(sl.domoticzSchemeIdx) + "'></td></tr>";
  }
  s += "</table>";
  s += "<p class='hint'>Vyber rezimu cez samostatne Domoticz IDx je dostupny len pre WS2812.</p>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Pomenovanie zariadenia</label>";
  s += "<label class='field'>Nazov (nadpis, zaklad .local adresy)</label>";
  s += "<input type='text' id='devname' maxlength='31' value='" + String(cfg.deviceName) + "'>";
  s += "</div>";

  s += "<button class='primary' onclick='saveAllAndRestart()'>Ulozit vsetko a restartovat</button>";
  s += "<p id='saveMsg' class='hint'></p>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>WiFi</label>";
  s += "<p class='hint'>Otvori konfiguracny hotspot pre zmenu WiFi siete.</p>";
  s += "<button class='primary' onclick='resetWifi()'>Zmenit WiFi siet</button>";
  s += "<p id='wifiMsg' class='hint'></p></div>";

  s += "<a class='navlink' href='/'>&#8592; Spat</a>";

  s += R"JS(<script>
const $ = id => document.getElementById(id);
function toggleWs2812Extra(ch){
  var row = $('wsrow'+ch);
  var func = $('func'+ch).value;
  row.style.display = (func == '3') ? 'table-row' : 'none';
}
function saveAllAndRestart(){
  $('saveMsg').textContent='Uklada sa...';
  var body = 'host='+encodeURIComponent($('host').value)
    +'&port='+encodeURIComponent($('port').value)
    +'&user='+encodeURIComponent($('user').value)
    +'&pass='+encodeURIComponent($('pass').value)
    +'&devname='+encodeURIComponent($('devname').value);
  for (var i=0;i<4;i++){
    body += '&gpio'+i+'='+encodeURIComponent($('gpio'+i).value);
    body += '&func'+i+'='+encodeURIComponent($('func'+i).value);
    body += '&idx'+i+'='+encodeURIComponent($('idx'+i).value);
    body += '&sidx'+i+'='+encodeURIComponent($('sidx'+i).value);
    body += '&lc'+i+'='+encodeURIComponent($('lc'+i).value);
  }
  fetch('/settingssave',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
    .then(function(){ return fetch('/restart',{method:'POST'}); })
    .then(function(){ document.body.innerHTML="<div class='wrap'><h1>&#9670; Restart...</h1><p style='text-align:center;color:#5c7c8f'>Zariadenie sa restartuje, obnov stranku o par sekund.</p></div>"; });
}
function resetWifi(){
  $('wifiMsg').textContent='Zariadenie prepina do konfiguracneho rezimu...';
  fetch('/wifi-reset',{method:'POST'}).then(function(){
    $('wifiMsg').textContent='Pripoj sa na hotspot a nastav siet znova. Tato stranka uz nebude reagovat.';
  });
}
</script>)JS";

  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

static void handleSettingsSave() {
  if (server.hasArg("host"))    strncpy(cfg.mqttHost, server.arg("host").c_str(), sizeof(cfg.mqttHost) - 1);
  if (server.hasArg("port"))    cfg.mqttPort = (uint16_t)server.arg("port").toInt();
  if (server.hasArg("user"))    strncpy(cfg.mqttUser, server.arg("user").c_str(), sizeof(cfg.mqttUser) - 1);
  if (server.hasArg("pass"))    strncpy(cfg.mqttPass, server.arg("pass").c_str(), sizeof(cfg.mqttPass) - 1);
  if (server.hasArg("devname")) strncpy(cfg.deviceName, server.arg("devname").c_str(), sizeof(cfg.deviceName) - 1);

  for (uint8_t ch = 0; ch < NUM_SLOTS; ch++) {
    SlotConfig &sl = cfg.slots[ch];
    char key[8];
    snprintf(key, sizeof(key), "gpio%d", ch); if (server.hasArg(key)) sl.gpio = (uint8_t)server.arg(key).toInt();
    snprintf(key, sizeof(key), "func%d", ch); if (server.hasArg(key)) sl.function = (uint8_t)server.arg(key).toInt();
    snprintf(key, sizeof(key), "idx%d", ch);  if (server.hasArg(key)) sl.domoticzIdx = server.arg(key).toInt();
    snprintf(key, sizeof(key), "sidx%d", ch); if (server.hasArg(key)) sl.domoticzSchemeIdx = server.arg(key).toInt();
    snprintf(key, sizeof(key), "lc%d", ch);   if (server.hasArg(key)) sl.ledCount = (uint16_t)server.arg(key).toInt();
  }

  configSave();
  server.send(200, "text/plain", "OK");
}

static void handleWifiReset() {
  server.send(200, "text/plain", "OK");
  delay(300);
  webServerStop();
  wifiStartConfigPortal();
  webServerBegin();
}

// ---------------------------------------------------------------------
// "/debug"
// ---------------------------------------------------------------------
static void handleDebug() {
  String s = pageOpen("Diagnostika");
  s += "<h1>&#128269; Diagnostika</h1><div class='subtitle'>Stav pinov</div>";

  for (uint8_t ch = 0; ch < NUM_SLOTS; ch++) {
    SlotConfig &sl = cfg.slots[ch];
    s += "<div class='panel'>";
    s += "<h2 class='chTitle'>Pin " + String(ch + 1) + " (GPIO" + String(sl.gpio) + ")</h2>";
    s += "<p class='hint'>Funkcia: <b>" + String(FUNC_NAMES[sl.function]) + "</b></p>";
    if (sl.function == FUNC_PWM) {
      s += "<p class='hint'>ledcAttach: <b style='color:";
      s += pwmSlotAttached(ch) ? "#33ff9d'>OK" : "#ff4d4d'>ZLYHALO";
      s += "</b></p>";
      s += "<p class='hint'>naposledy zapisana PWM hodnota: <b>" + String(pwmSlotLastDuty(ch)) + "</b> / 255</p>";
    }
    if (sl.function != FUNC_NONE && sl.function != FUNC_STATUS_LED) {
      s += "<p class='hint'>power=" + String(sl.power ? "true" : "false") + ", scheme=" + String(sl.scheme) + ", bright=" + String(sl.brightness) + "</p>";
    }
    s += "</div>";
  }
  int rssi = WiFi.RSSI();
  String rssiQuality = (rssi > -60) ? "vyborny" : (rssi > -70) ? "dobry" : (rssi > -80) ? "slaby" : "velmi slaby";
  s += "<p class='hint'>WiFi signal: <b>" + String(rssi) + " dBm</b> (" + rssiQuality + ")</p>";
  s += "<p class='hint'>Volna pamat: " + String(ESP.getFreeHeap()) + " B, beh: " + String(millis() / 1000) + " s</p>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>MQTT log</label>";
  s += "<p class='hint'>MQTT: <b>" + String(mqttIsConnected() ? "pripojene" : "nepripojene") + "</b></p>";
  s += "<p class='hint'>" + mqttDebugLog() + "</p>";
  s += "<p class='hint'>Obnov stranku (F5) po vyskusani ovladania z Domoticz, nech uvidis najnovsie udalosti.</p>";
  s += "</div>";

  s += "<a class='navlink' href='/'>&#8592; Spat</a>";
  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

// ---------------------------------------------------------------------
// "/update" - PIN je sucastou toho isteho formulara ako vyber suboru
// ---------------------------------------------------------------------
static void handleUpdatePage() {
  String s = pageOpen("Aktualizacia firmveru");
  s += "<h1>&#8657; Update</h1><div class='subtitle'>Aktualizacia firmveru</div>";
  s += "<div class='panel'>";
  s += "<p class='hint'>Nahraj skompilovany .bin subor (Arduino IDE &rarr; Sketch &rarr; Export compiled binary).</p>";
  s += "<form method='POST' action='/dofirmwareupdate' enctype='multipart/form-data'>";
  s += "<label class='field' style='margin-top:0'>PIN kod</label><input type='password' name='pin' required>";
  s += "<label class='field'>Subor firmveru (.bin)</label><input type='file' name='update' accept='.bin' required>";
  s += "<button class='primary' type='submit'>Nahrat a aktualizovat</button>";
  s += "</form></div>";
  s += "<a class='navlink' href='/'>&#8592; Spat</a>";
  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

static void handleDoUpdateResult() {
  server.sendHeader("Connection", "close");
  if (!otaPinOk) {
    String s = pageOpen("Chyba");
    s += "<h1>&#10060; Nespravny PIN</h1><div class='panel'><p class='hint'>Aktualizacia bola zamietnuta.</p>";
    s += "<a class='navlink' href='/update'>Skus znova</a></div>" + String(PAGE_CLOSE);
    server.send(403, "text/html", s);
    return;
  }
  server.send(200, "text/plain", Update.hasError() ? "CHYBA pri aktualizacii" : "OK - zariadenie sa restartuje");
  delay(500);
  ESP.restart();
}

static void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaPinOk = (server.arg("pin") == String(cfg.otaPin));
    if (otaPinOk) Update.begin(ESP.getFreeSketchSpace()); // ESP8266 nema UPDATE_SIZE_UNKNOWN, treba explicitnu velkost
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaPinOk) Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaPinOk) Update.end(true);
  }
}

void webServerBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/live", HTTP_POST, handleLive);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/settingssave", HTTP_POST, handleSettingsSave);
  server.on("/wifi-reset", HTTP_POST, handleWifiReset);
  server.on("/debug", HTTP_GET, handleDebug);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/dofirmwareupdate", HTTP_POST, handleDoUpdateResult, handleUpdateUpload);
  server.begin();
}

void webServerStop() { server.stop(); }
void webServerLoop() { server.handleClient(); }
