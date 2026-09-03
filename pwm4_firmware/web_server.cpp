#include "web_server.h"
#include "config.h"
#include "pwm_lights.h"
#include "mqtt_client.h"
#include "wifi_setup.h"
#include <WebServer.h>
#include <Update.h>

static WebServer server(80);
static bool otaPinOk = false;

// ---------------------------------------------------------------------
// Spolocny vizualny styl (rovnaky ako pri WS2812 lampe, pre konzistenciu)
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
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
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

static const char* SCHEME_NAMES[PWM_SCHEME_COUNT] = {
  "0 - Staticka", "1 - Wakeup (nabeh/zhasnutie)", "2 - Pulzovanie", "3 - Nahodne", "4 - Sviecka"
};

// ---------------------------------------------------------------------
// "/" - hlavna stranka, 4 nezavisle panely
// ---------------------------------------------------------------------
static void handleRoot() {
  String s = pageOpen("Svetla // Ovladaci panel");
  s.reserve(s.length() + 6000);
  s += "<h1>&#9670; " + String(cfg.deviceName) + "</h1><div class='subtitle'>Ovladaci panel</div>";
  s += "<div class='status-line'><span><span class='dot ";
  s += (mqttIsConnected() ? "on" : "off");
  s += "'></span>MQTT: <b>" + String(mqttIsConnected() ? "pripojene" : "-") + "</b></span>";
  s += "<span>IP: <b>" + wifiGetIp() + "</b></span></div>";

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    s += "<div class='panel'>";
    s += "<div class='row'><h2 class='chTitle'>Kanal " + String(ch + 1) + "</h2>";
    s += "<label class='switch'><input type='checkbox' class='pwrToggle' data-ch='" + String(ch) + "'";
    s += (cfg.power[ch] ? " checked" : "");
    s += "><span class='slider-toggle'></span></label></div>";

    s += "<label class='field'>Rezim</label><select class='schemeSel' data-ch='" + String(ch) + "'>";
    for (uint8_t i = 0; i < PWM_SCHEME_COUNT; i++) {
      s += "<option value='" + String(i) + "'" + (cfg.scheme[ch] == i ? " selected" : "") + ">" + SCHEME_NAMES[i] + "</option>";
    }
    s += "</select>";

    s += "<label class='field'>Jas <span class='value-tag brightVal' data-ch='" + String(ch) + "'>" + String(cfg.brightness[ch]) + "</span></label>";
    s += "<input type='range' min='0' max='255' class='brightSl' data-ch='" + String(ch) + "' value='" + String(cfg.brightness[ch]) + "'>";

    s += "<label class='field'>Rychlost efektu <span class='value-tag speedVal' data-ch='" + String(ch) + "'>" + String(cfg.speed[ch]) + "</span></label>";
    s += "<input type='range' min='1' max='20' class='speedSl' data-ch='" + String(ch) + "' value='" + String(cfg.speed[ch]) + "'>";
    s += "</div>";
  }

  s += "<a class='navlink' href='/settings'>&#9881; Nastavenia</a>";
  s += "<a class='navlink' href='/debug'>&#128269; Diagnostika</a>";
  s += "<a class='navlink' href='/update'>&#8657; Aktualizacia firmveru</a>";

  s += R"JS(<script>
const $$ = sel => document.querySelectorAll(sel);
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
    sendLive(e.target.dataset.ch, 'bright', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'bright='+e.target.value); });
});
$$('.speedSl').forEach(function(el){
  el.addEventListener('input', function(e){
    var v=document.querySelector(".speedVal[data-ch='"+e.target.dataset.ch+"']");
    if(v) v.textContent=e.target.value;
    sendLive(e.target.dataset.ch, 'speed', e.target.value);
  });
  el.addEventListener('change', function(e){ sendSet(e.target.dataset.ch, 'speed='+e.target.value); });
});
</script>)JS";

  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

static int8_t argChannel() {
  if (!server.hasArg("ch")) return -1;
  int ch = server.arg("ch").toInt();
  if (ch < 0 || ch >= NUM_CHANNELS) return -1;
  return (int8_t)ch;
}

// live = zmena v RAM bez zapisu do flash (pocas tahania posuvnika)
static void handleLive() {
  int8_t ch = argChannel();
  if (ch < 0) { Serial.println("[WEB] /live: chybny alebo chybajuci parameter 'ch'"); server.send(400, "text/plain", "chybny kanal"); return; }
  if (server.hasArg("bright")) cfg.brightness[ch] = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("speed"))  cfg.speed[ch] = (uint8_t)server.arg("speed").toInt();
  Serial.printf("[WEB] /live kanal %d: bright=%u speed=%u\n", ch, cfg.brightness[ch], cfg.speed[ch]);
  server.send(200, "text/plain", "OK");
}

// set = aplikuje a ULOZI do flash + publikuje na MQTT
static void handleSet() {
  int8_t ch = argChannel();
  if (ch < 0) { Serial.println("[WEB] /set: chybny alebo chybajuci parameter 'ch'"); server.send(400, "text/plain", "chybny kanal"); return; }

  if (server.hasArg("power"))  { cfg.power[ch] = server.arg("power").toInt() != 0; pwmApplyPower(ch); }
  if (server.hasArg("scheme")) { cfg.scheme[ch] = (uint8_t)server.arg("scheme").toInt(); }
  if (server.hasArg("bright")) cfg.brightness[ch] = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("speed"))  cfg.speed[ch] = (uint8_t)server.arg("speed").toInt();

  Serial.printf("[WEB] /set kanal %d: power=%d scheme=%u bright=%u speed=%u\n", ch, cfg.power[ch], cfg.scheme[ch], cfg.brightness[ch], cfg.speed[ch]);

  configSave();
  mqttPublishChannel(ch);
  server.send(200, "text/plain", "OK");
}

static void handleRestart() {
  server.send(200, "text/plain", "OK");
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------
// "/debug" - diagnostika PWM kanalov priamo cez web (nahradza Serial monitor)
// ---------------------------------------------------------------------
static const uint8_t DEBUG_PINS[NUM_CHANNELS] = {0, 1, 2, 3};

static void handleDebug() {
  String s = pageOpen("Diagnostika");
  s += "<h1>&#128269; Diagnostika</h1><div class='subtitle'>Stav PWM kanalov</div>";

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    s += "<div class='panel'>";
    s += "<h2 class='chTitle'>Kanal " + String(ch + 1) + " (GPIO" + String(DEBUG_PINS[ch]) + ")</h2>";
    s += "<p class='hint'>ledcAttach pri starte: <b style='color:";
    s += pwmChannelAttached(ch) ? "#33ff9d'>OK" : "#ff4d4d'>ZLYHALO";
    s += "</b></p>";
    s += "<p class='hint'>cfg.power = " + String(cfg.power[ch] ? "true" : "false") + "</p>";
    s += "<p class='hint'>cfg.scheme = " + String(cfg.scheme[ch]) + "</p>";
    s += "<p class='hint'>cfg.brightness = " + String(cfg.brightness[ch]) + " / 255</p>";
    s += "<p class='hint'>naposledy zapisana PWM hodnota (lastLevel) = <b>" + String(pwmLastDuty(ch)) + "</b> / 255</p>";
    s += "</div>";
  }

  s += "<p class='hint'>Voľna pamat: " + String(ESP.getFreeHeap()) + " B</p>";
  s += "<p class='hint'>Beh od startu: " + String(millis() / 1000) + " s</p>";
  s += "<a class='navlink' href='/'>&#8592; Spat</a>";
  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

// ---------------------------------------------------------------------
// "/settings"
// ---------------------------------------------------------------------
static void handleSettingsPage() {
  String s = pageOpen("Nastavenia");
  s.reserve(s.length() + 4200);
  s += "<h1>&#9881; Nastavenia</h1><div class='subtitle'>" + String(cfg.deviceName) + "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>MQTT</label>";
  s += "<label class='field'>Broker (IP alebo hostname)</label><input id='host' value='" + String(cfg.mqttHost) + "'>";
  s += "<label class='field'>Port</label><input id='port' value='" + String(cfg.mqttPort) + "'>";
  s += "<label class='field'>Pouzivatelske meno (nepovinne)</label><input id='user' value='" + String(cfg.mqttUser) + "'>";
  s += "<label class='field'>Heslo (nepovinne)</label><input type='password' id='pass' value='" + String(cfg.mqttPass) + "'>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Domoticz</label>";
  s += "<p class='hint'>Kazdy kanal = samostatne virtualne zariadenie typu \"Dimmer\" v Domoticz.</p>";
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    s += "<label class='field'>IDx kanal " + String(ch + 1) + "</label><input id='idx" + String(ch) + "' value='" + String(cfg.domoticzIdx[ch]) + "'>";
  }
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
function saveAllAndRestart(){
  $('saveMsg').textContent='Uklada sa...';
  var body = 'host='+encodeURIComponent($('host').value)
    +'&port='+encodeURIComponent($('port').value)
    +'&user='+encodeURIComponent($('user').value)
    +'&pass='+encodeURIComponent($('pass').value)
    +'&devname='+encodeURIComponent($('devname').value);
  for (var i=0;i<4;i++){ body += '&idx'+i+'='+encodeURIComponent($('idx'+i).value); }
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
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    char key[8];
    snprintf(key, sizeof(key), "idx%d", ch);
    if (server.hasArg(key)) cfg.domoticzIdx[ch] = server.arg(key).toInt();
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
    if (otaPinOk) Update.begin(UPDATE_SIZE_UNKNOWN);
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
  server.on("/debug", HTTP_GET, handleDebug);
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/settingssave", HTTP_POST, handleSettingsSave);
  server.on("/wifi-reset", HTTP_POST, handleWifiReset);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/dofirmwareupdate", HTTP_POST, handleDoUpdateResult, handleUpdateUpload);
  server.begin();
}

void webServerStop() { server.stop(); }
void webServerLoop() { server.handleClient(); }
