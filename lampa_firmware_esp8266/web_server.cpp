#include "web_server.h"
#include "config.h"
#include "led_control.h"
#include "mqtt_client.h"
#include "wifi_setup.h"
#include "event_log.h"
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ESP8266WiFi.h>

static ESP8266WebServer server(80);

// PIN sa overuje priamo v ramci upload formulara (viz handleUpdateUpload) -
// ziadne prehliadacove basic-auth okno, ziadne pole "meno".
static bool otaPinOk = false;

// ---------------------------------------------------------------------
// Spolocny vizualny styl vsetkych stranok
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
input[type=range]#hue{background:linear-gradient(90deg,red,yellow,lime,cyan,blue,magenta,red);}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;
  background:radial-gradient(circle at 35% 35%,#fff,var(--accent));box-shadow:0 0 10px var(--accent);
  cursor:pointer;border:none;}
.colorPreview{width:100%;height:52px;border-radius:10px;margin-top:8px;border:1px solid var(--panel-border);
  transition:background .08s linear,box-shadow .08s linear;}
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
  s.reserve(3200); // CSS blok ma cca 2.5kB - rezervujeme naraz, nech sa String neprealokuva po kuskoch
  s += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<title>" + title + "</title><style>";
  s += FPSTR(PAGE_STYLE);
  s += "</style></head><body><div class='wrap'>";
  return s;
}
static const char PAGE_CLOSE[] = "</div></body></html>";

// ---------------------------------------------------------------------
// "/" - hlavna stranka
// ---------------------------------------------------------------------
static void handleRoot() {
  String s = pageOpen("Lampa // Ovladaci panel");
  s.reserve(s.length() + 3500); // zvysok stranky (JS + ovladacie prvky) - jedna velka rezervacia
  s += "<h1>&#9670; " + String(cfg.deviceName) + "</h1><div class='subtitle'>Ovladaci panel</div>";

  s += "<div class='panel'>";
  s += "<div class='status-line'><span><span class='dot ";
  s += (mqttIsConnected() ? "on" : "off");
  s += "'></span>MQTT: <b>" + String(mqttIsConnected() ? "pripojene" : "-") + "</b></span>";
  s += "<span>IP: <b>" + wifiGetIp() + "</b></span></div>";

  s += "<div class='row'><label class='field' style='margin:0'>Napajanie</label>";
  s += "<label class='switch'><input type='checkbox' id='powerToggle'";
  s += (cfg.power ? " checked" : "");
  s += "><span class='slider-toggle'></span></label></div>";

  s += "<label class='field'>Rezim svietenia</label><select id='scheme'>";
  const char* names[] = {"0 - Pevna farba", "1 - Wakeup (dychovy cyklus)", "2 - Cyklus farieb hore",
                          "3 - Cyklus farieb dole", "4 - Nahodne farby", "6 - Sviecka (plamen)",
                          "7 - RGB vzor", "11 - Duha"};
  const uint8_t vals[] = {0, 1, 2, 3, 4, 6, 7, 11};
  for (uint8_t i = 0; i < 8; i++) {
    s += "<option value='" + String(vals[i]) + "'" + (cfg.scheme == vals[i] ? " selected" : "") + ">" + names[i] + "</option>";
  }
  s += "</select>";

  s += "<label class='field'>Jas <span class='value-tag' id='brightVal'>" + String(cfg.brightness) + "</span></label>";
  s += "<input type='range' min='0' max='255' id='bright' value='" + String(cfg.brightness) + "'>";

  s += "<label class='field'>Sytost farby <span class='value-tag' id='satVal'>" + String(cfg.saturation) + "</span></label>";
  s += "<input type='range' min='0' max='255' id='sat' value='" + String(cfg.saturation) + "'>";

  s += "<label class='field'>Odtien farby <span class='value-tag' id='hueVal'>" + String(cfg.hue) + "</span></label>";
  s += "<input type='range' min='0' max='255' id='hue' value='" + String(cfg.hue) + "'>";
  s += "<div class='colorPreview' id='colorPreview'></div>";

  s += "<label class='field'>Rychlost efektov <span class='value-tag' id='speedVal'>" + String(cfg.speed) + "</span></label>";
  s += "<input type='range' min='1' max='20' id='speed' value='" + String(cfg.speed) + "'>";
  s += "</div>";

  s += "<a class='navlink' href='/settings'>&#9881; Nastavenia</a>";
  s += "<a class='navlink' href='/update'>&#8657; Aktualizacia firmveru</a>";

  s += R"JS(<script>
function hsv2rgb(h,s,v){h/=255;s/=255;v/=255;var i=Math.floor(h*6),f=h*6-i;
var p=v*(1-s),q=v*(1-f*s),t=v*(1-(1-f)*s),r,g,b;
switch(i%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;
case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;case 5:r=v;g=p;b=q;break;}
return [Math.round(r*255),Math.round(g*255),Math.round(b*255)];}
const $=id=>document.getElementById(id);
function updatePreview(){
  var h=+$('hue').value,s=+$('sat').value,v=+$('bright').value;
  var rgb=hsv2rgb(h,s,Math.max(v,40));
  var css='rgb('+rgb[0]+','+rgb[1]+','+rgb[2]+')';
  $('colorPreview').style.background=css;
  $('colorPreview').style.boxShadow='0 0 26px '+css;
}
let liveTimer=null;
function sendLive(field,value){
  clearTimeout(liveTimer);
  liveTimer=setTimeout(function(){
    fetch('/live',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:field+'='+value});
  },60);
}
function sendSet(body){
  fetch('/set',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body});
}
['bright','sat','hue','speed'].forEach(function(id){
  $(id).addEventListener('input',function(e){
    if($(id+'Val')) $(id+'Val').textContent=e.target.value;
    if(id!=='speed') updatePreview();
    sendLive(id,e.target.value);
  });
  $(id).addEventListener('change',function(e){ sendSet(id+'='+e.target.value); });
});
$('scheme').addEventListener('change',function(e){ sendSet('scheme='+e.target.value); });
$('powerToggle').addEventListener('change',function(e){ sendSet('power='+(e.target.checked?1:0)); });
updatePreview();
</script>)JS";

  s += PAGE_CLOSE;
  server.send(200, "text/html", s);
}

// live = aplikuje sa ihned do RAM, ziadny zapis do flash (pouziva sa pocas tahania posuvnika)
static void handleLive() {
  logSetSource("web");
  if (server.hasArg("bright")) cfg.brightness = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("sat"))    cfg.saturation = (uint8_t)server.arg("sat").toInt();
  if (server.hasArg("hue"))    cfg.hue        = (uint8_t)server.arg("hue").toInt();
  if (server.hasArg("speed"))  cfg.speed      = (uint8_t)server.arg("speed").toInt();
  server.send(200, "text/plain", "OK");
}

// set = aplikuje a ULOZI do flash (pouziva sa pri pusteni posuvnika / zmene rezimu / prepnuti napajania)
static void handleSet() {
  logSetSource("web");
  if (server.hasArg("power"))    { cfg.power = server.arg("power").toInt() != 0; ledApplyPower(); mqttPublishState(cfg.power); }
  if (server.hasArg("scheme"))   { cfg.scheme = (uint8_t)server.arg("scheme").toInt(); }
  if (server.hasArg("bright"))   cfg.brightness = (uint8_t)server.arg("bright").toInt();
  if (server.hasArg("sat"))      cfg.saturation = (uint8_t)server.arg("sat").toInt();
  if (server.hasArg("hue"))      cfg.hue        = (uint8_t)server.arg("hue").toInt();
  if (server.hasArg("speed"))    cfg.speed      = (uint8_t)server.arg("speed").toInt();
  if (server.hasArg("ledcount")) cfg.ledCount   = (uint16_t)server.arg("ledcount").toInt();

  configSave();

  if (server.hasArg("bright") || server.hasArg("sat") || server.hasArg("hue")) mqttPublishColor();
  if (server.hasArg("scheme")) mqttPublishScheme();

  server.send(200, "text/plain", "OK");
}

static void handleRestart() {
  logEvent("RESTART: z webu");
  server.send(200, "text/plain", "OK");
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------
// "/settings" - MQTT, Domoticz, pomenovanie zariadenia, pocet LED - VSETKO
// sa uklada jednym tlacidlom naraz. Zmena WiFi je samostatna akcia (nie
// ukladanie hodnoty, ale prepnutie do config rezimu), preto ma vlastne tlacidlo.
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// Info o zariadeni (RSSI, verzia FW, cip...) - JSON pre /info + panel v nastaveniach
// ---------------------------------------------------------------------
static const char* rssiQuality(int rssi) {
  if (rssi >= -55) return "vyborny";
  if (rssi >= -67) return "dobry";
  if (rssi >= -75) return "slaby";
  if (rssi >= -85) return "velmi slaby";
  return "kriticky";
}

static String uptimeStr() {
  uint32_t t = millis() / 1000;
  char b[32];
  snprintf(b, sizeof(b), "%ud %02uh %02um", (unsigned)(t / 86400), (unsigned)((t / 3600) % 24), (unsigned)((t / 60) % 60));
  return String(b);
}

static void handleInfoJson() {
  int rssi = WiFi.RSSI();
  String j;
  j.reserve(520);
  j += "{\"rssi\":" + String(rssi);
  j += ",\"quality\":\"" + String(rssiQuality(rssi)) + "\"";
  j += ",\"ssid\":\"" + WiFi.SSID() + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"uptime\":\"" + uptimeStr() + "\"";
  j += ",\"mqtt\":\"" + String(strlen(cfg.mqttHost) == 0 ? "vypnute" : (mqttIsConnected() ? "pripojene" : "nepripojene (rc=" + String(mqttLastState()) + ")")) + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

static String infoRow(const char* k, const String &v, const char* id = nullptr) {
  String r = "<div style='display:flex;justify-content:space-between;gap:10px;padding:5px 0;border-bottom:1px solid var(--panel-border);font-size:13px'>";
  r += "<span style='color:var(--text-dim)'>" + String(k) + "</span><span";
  if (id) r += " id='" + String(id) + "'";
  r += " style='text-align:right;word-break:break-all'>" + v + "</span></div>";
  return r;
}

static String buildInfoPanel() {
  int rssi = WiFi.RSSI();
  String s;
  s.reserve(1800);
  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Info o zariadeni</label>";
  s += infoRow("Firmware", String(FW_NAME) + " v" + FW_VERSION);
  s += infoRow("Zostavene", String(__DATE__) + " " + __TIME__);
  s += infoRow("Cip", String("ESP8266, ") + String(ESP.getChipId(), HEX) + ", " + String(ESP.getCpuFreqMHz()) + " MHz");
  s += infoRow("Flash", String(ESP.getFlashChipRealSize() / 1024) + " kB (sketch " + String(ESP.getSketchSize() / 1024) + " kB)");
  s += infoRow("Core / SDK", String(ESP.getCoreVersion()) + " / " + ESP.getSdkVersion());
  s += infoRow("Dovod posl. restartu", ESP.getResetReason());
  s += infoRow("MAC", WiFi.macAddress());
  s += infoRow("WiFi siet", WiFi.SSID(), "iSsid");
  s += infoRow("Sila signalu", String(rssi) + " dBm (" + rssiQuality(rssi) + ")", "iRssi");
  s += infoRow("IP adresa", WiFi.localIP().toString(), "iIp");
  s += infoRow("Volna pamat", String(ESP.getFreeHeap()) + " B", "iHeap");
  s += infoRow("MQTT", strlen(cfg.mqttHost) == 0 ? String("vypnute") : (mqttIsConnected() ? String("pripojene") : String("nepripojene (rc=") + mqttLastState() + ")"), "iMqtt");
  s += infoRow("Beh", uptimeStr(), "iUp");
  s += "<p class='hint'>Sila signalu: lepsie nez -67 dBm je dobre; horsie nez -75 dBm sposobuje vypadky a pomale nacitanie stranky.</p>";
  s += "</div>";
  return s;
}


// ---------------------------------------------------------------------
// "/log" - udalostny log (HTML) a "/log.txt" (cisty text na skopirovanie)
// ---------------------------------------------------------------------
struct LogCtx { bool txt; bool sepDone; };

static void logLineSender(const char* line, bool prev, void* c) {
  LogCtx* ctx = (LogCtx*)c;
  if (prev && !ctx->sepDone) {
    ctx->sepDone = true;
    server.sendContent(ctx->txt ? "---- pred poslednym restartom ----\n" : "<div class='hint' style='margin:10px 0 4px;color:var(--accent2)'>&#8212; pred poslednym restartom &#8212;</div>");
  }
  if (ctx->txt) {
    server.sendContent(String(line) + "\n");
  } else {
    String esc = line;
    esc.replace("&", "&amp;");
    esc.replace("<", "&lt;");
    esc.replace(">", "&gt;");
    server.sendContent("<div style='font-size:12px;padding:2px 0;border-bottom:1px solid var(--panel-border);word-break:break-word'>" + esc + "</div>");
  }
}

static void handleLogPage() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(pageOpen("Log udalosti"));
  server.sendContent("<h1>&#9776; Log udalosti</h1><div class='subtitle'>" + String(cfg.deviceName) + " &middot; boot #" + String(logBootCount()) + " &middot; " + logResetReason() + "</div>");
  server.sendContent("<div class='panel'><button class='primary' onclick='location.reload()'>Obnovit</button>"
                     "<a class='navlink' href='/log.txt'>Text (na skopirovanie)</a><div style='margin-top:10px'>");
  LogCtx ctx = {false, false};
  logForEach(logLineSender, &ctx);
  server.sendContent("</div></div><a class='navlink' href='/settings'>&#8592; Nastavenia</a>");
  server.sendContent(PAGE_CLOSE);
  server.sendContent("");
}

static void handleLogTxt() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain; charset=utf-8", "");
  LogCtx ctx = {true, false};
  logForEach(logLineSender, &ctx);
  server.sendContent("");
}

static void handleSettingsPage() {
  String s = pageOpen("Nastavenia");
  s.reserve(s.length() + 3800);
  s += "<h1>&#9881; Nastavenia</h1><div class='subtitle'>" + String(cfg.deviceName) + "</div>";

  s += buildInfoPanel();

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>MQTT</label>";
  s += "<label class='field'>Broker (IP alebo hostname)</label><input id='host' value='" + String(cfg.mqttHost) + "'>";
  s += "<label class='field'>Port</label><input id='port' value='" + String(cfg.mqttPort) + "'>";
  s += "<label class='field'>Pouzivatelske meno (nepovinne)</label><input id='user' value='" + String(cfg.mqttUser) + "'>";
  s += "<label class='field'>Heslo (nepovinne)</label><input type='password' id='pass' value='" + String(cfg.mqttPass) + "'>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Domoticz</label>";
  s += "<label class='field'>IDx svetla (On/Off + RGB farba, typ &quot;Color Switch&quot;)</label><input id='idx' value='" + String(cfg.domoticzIdx) + "'>";
  s += "<label class='field'>IDx pre vyber rezimu (typ &quot;Selector Switch&quot;)</label><input id='schemeidx' value='" + String(cfg.domoticzSchemeIdx) + "'>";
  s += "<p class='hint'>Uroven Selector Switch-a musi mat 8 pomenovani v poradi: Pevna farba, Wakeup, Cyklus hore, Cyklus dole, Nahodne, Sviecka, RGB vzor, Duha.</p>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>Pomenovanie zariadenia</label>";
  s += "<label class='field'>Nazov (nadpis na strankach, zaklad .local adresy)</label>";
  s += "<input type='text' id='devname' maxlength='31' value='" + String(cfg.deviceName) + "'>";
  s += "</div>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>LED pas</label>";
  s += "<label class='field'>Pocet LED</label>";
  s += "<input type='number' min='1' max='500' id='ledcount' value='" + String(cfg.ledCount) + "'>";
  s += "</div>";

  s += "<button class='primary' onclick='saveAllAndRestart()'>Ulozit vsetko a restartovat</button>";
  s += "<p id='saveMsg' class='hint'></p>";

  s += "<div class='panel'><label class='field' style='margin-top:0;color:var(--accent)'>WiFi</label>";
  s += "<p class='hint'>Otvori konfiguracny hotspot pre zmenu WiFi siete (rovnake ako 3x klik na tlacidle).</p>";
  s += "<button class='primary' onclick='resetWifi()'>Zmenit WiFi siet</button>";
  s += "<p id='wifiMsg' class='hint'></p></div>";

  s += "<a class='navlink' href='/log'>&#9776; Log udalosti</a>";
  s += "<a class='navlink' href='/'>&#8592; Spat</a>";

  s += R"JS(<script>
const $ = id => document.getElementById(id);
function refreshInfo(){
  fetch('/info').then(function(r){return r.json();}).then(function(d){
    $('iRssi').textContent=d.rssi+' dBm ('+d.quality+')';
    $('iSsid').textContent=d.ssid; $('iIp').textContent=d.ip;
    $('iHeap').textContent=d.heap+' B'; $('iMqtt').textContent=d.mqtt; $('iUp').textContent=d.uptime;
  }).catch(function(){});
}
setInterval(refreshInfo,5000);
function saveAllAndRestart(){
  $('saveMsg').textContent='Uklada sa...';
  var body = 'host='+encodeURIComponent($('host').value)
    +'&port='+encodeURIComponent($('port').value)
    +'&user='+encodeURIComponent($('user').value)
    +'&pass='+encodeURIComponent($('pass').value)
    +'&idx='+encodeURIComponent($('idx').value)
    +'&schemeidx='+encodeURIComponent($('schemeidx').value)
    +'&devname='+encodeURIComponent($('devname').value)
    +'&ledcount='+encodeURIComponent($('ledcount').value);
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
  if (server.hasArg("idx"))     cfg.domoticzIdx = server.arg("idx").toInt();
  if (server.hasArg("schemeidx")) cfg.domoticzSchemeIdx = server.arg("schemeidx").toInt();
  if (server.hasArg("devname")) strncpy(cfg.deviceName, server.arg("devname").c_str(), sizeof(cfg.deviceName) - 1);
  if (server.hasArg("ledcount")) cfg.ledCount = (uint16_t)server.arg("ledcount").toInt();

  configSave();
  server.send(200, "text/plain", "OK");
  // mqttBegin() sa nevola tu - aj tak nasleduje restart, ktory nacita vsetko od znova
}

static void handleWifiReset() {
  server.send(200, "text/plain", "OK");
  delay(300);
  webServerStop();
  wifiStartConfigPortal();   // blokuje - device sa prepne do AP rezimu, spojenie s prehliadacom padne
  webServerBegin();
}

// ---------------------------------------------------------------------
// "/update" - PIN je sucastou toho isteho formulara ako vyber suboru,
// ziadne prehliadacove basic-auth okno, ziadne pole "meno".
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
    // Pole 'pin' je vo formulari PRED polom 'update', takze uz je v tomto bode dostupne.
    otaPinOk = (server.arg("pin") == String(cfg.otaPin));
    if (otaPinOk) Update.begin(ESP.getFreeSketchSpace()); // ESP8266 nema UPDATE_SIZE_UNKNOWN, treba explicitnu velkost
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaPinOk) Update.write(upload.buf, upload.currentSize);
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaPinOk) Update.end(true);
  }
}

void webServerBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/live", HTTP_POST, handleLive);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/mqtt", HTTP_GET, handleSettingsPage); // stary odkaz presmerovany na novu stranku
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/info", HTTP_GET, handleInfoJson);
  server.on("/log", HTTP_GET, handleLogPage);
  server.on("/log.txt", HTTP_GET, handleLogTxt);
  server.on("/settingssave", HTTP_POST, handleSettingsSave);
  server.on("/wifi-reset", HTTP_POST, handleWifiReset);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/dofirmwareupdate", HTTP_POST, handleDoUpdateResult, handleUpdateUpload);
  server.begin();
}

void webServerStop() { server.stop(); }
void webServerLoop() { server.handleClient(); }
