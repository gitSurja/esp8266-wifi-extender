#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
extern "C" {
#include "user_interface.h"
}

#define AP_PASS "88888888"
#define EEPROM_SIZE 256
#define CREDENTIAL_MAGIC 0x4D495345

#define NAPT_ENTRIES 512
#define NAPT_PORTS 16

ESP8266WebServer server(80);
volatile unsigned long builtinTestUntil = 0;
volatile int eventTestPulses = 0;
bool eventTestOn = false;
unsigned long eventTestAt = 0;
unsigned long lastReconnectAttempt = 0;
int activeApChannel = 0;
String staSsid = "MIS";
String staPass = "CHANGE_ME";
String apSsid = "MIS_EXT";

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  uint32_t magic = 0;
  EEPROM.get(0, magic);
  if (magic != CREDENTIAL_MAGIC) return;
  char ssid[65] = {}, pass[65] = {};
  EEPROM.get(4, ssid); EEPROM.get(69, pass);
  if (ssid[0]) { staSsid = ssid; staPass = pass; }
}

void saveCredentials(const String& ssid, const String& pass) {
  uint32_t magic = CREDENTIAL_MAGIC;
  char ssidBuf[65] = {}, passBuf[65] = {};
  ssid.substring(0, 64).toCharArray(ssidBuf, sizeof(ssidBuf));
  pass.substring(0, 64).toCharArray(passBuf, sizeof(passBuf));
  EEPROM.put(0, magic); EEPROM.put(4, ssidBuf); EEPROM.put(69, passBuf); EEPROM.commit();
}

// QR for WIFI:T:WPA;S:MIS_EXT;P:88888888;;
const char QR_B64[] = "iVBORw0KGgoAAAANSUhEUgAAAHwAAAB8AQAAAACDZekTAAABFklEQVR4nK2V0YkEMQxD5WH/5Q62/7LSgVyBDmXvZ/8Oc2YIzsA8ZI/ilPEV83zvgT+8mKpKUt03XTBeMAY9dnmSbnRgCiPNCEl3jBtN9NxsVUuiXSS2jBIRAQ0BSVc6CvlcFwhowYBtyyTFm24YUjgUr5YdwzCzgJTEHUOQU4aCWdbClBENgrzrqZTHdgi7fjwR0ULFqVBv/AFZMuMQi1r2wyYEWra3PQVvL2MOct1ThhKbiTt/yIy1fpVYy7PPFsYDz+7c4vNfrl2x9MeLmRoHFc7sZtCTYc4G+txZtJuntiZiMlPfS8bc6+WcVjVWXn/uevAm29reL4nG6QP0cMdgZs/7c3y56sd1xcel8evKH/UP9/4PK1S/CHy7244AAAAASUVORK5CYII=";

String macStr(uint8_t *mac) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(b);
}

String clientRows() {
  String r = "";
  struct station_info *info = wifi_softap_get_station_info();
  struct station_info *p = info;
  int i = 1;
  while (p) {
    uint32_t ip = p->ip.addr;
    IPAddress a((ip) & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    String mac = macStr(p->bssid);
    r += "<tr><td>" + String(i) + "</td><td>" + mac + "</td><td>" + a.toString() + "</td><td><canvas class='devchart' data-mac='" + mac + "' width='180' height='36'></canvas></td></tr>";
    p = STAILQ_NEXT(p, next);
    i++;
  }
  wifi_softap_free_station_info();
  if (i == 1) r = "<tr><td colspan='3'>No devices connected</td></tr>";
  return r;
}

String page() {
  String s = R"rawliteral(<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>MIS_EXT Control</title><style>
:root{--bg:#f7f8fa;--card:#fff;--line:#dfe3e8;--text:#11151c;--muted:#68717d;--good:#12b981;--blue:#2675e8}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px Arial,system-ui,sans-serif}.layout{min-height:100vh}main{max-width:1440px;width:100%;margin:auto;padding:20px 26px}.top{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-bottom:20px;border-bottom:1px solid var(--line);padding-bottom:18px}h1{font-size:23px;margin:0}h2{font-size:16px;margin:0 0 15px}.muted{color:var(--muted)}
.pill{border-radius:20px;padding:8px 13px;font-weight:700;background:#eef1f5}.up{color:#08794f;background:#c9f5e3}.down{color:#a72335;background:#fde0e5}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px}.card{background:var(--card);border:1px solid #cfd5dc;border-radius:5px;padding:16px;margin-bottom:0;box-shadow:0 1px 2px #00000008}.metric b{display:block;font-size:28px;margin-top:8px}.metric span{color:#222;font-weight:700}.wide{grid-column:span 2}.chart{height:170px;width:100%;display:block}.devchart{width:180px;height:40px;border:1px solid #e1e5ea;border-radius:4px;background:#f8fafc;vertical-align:middle}.links{display:flex;flex-wrap:wrap;gap:10px}.links a,.links button{color:#fff;text-decoration:none;background:#151a22;padding:10px 14px;border-radius:4px;border:0;font:inherit;cursor:pointer}table{width:100%;border-collapse:collapse}td,th{text-align:left;border-bottom:1px solid var(--line);padding:10px 5px;font-size:13px}th{color:var(--muted);background:#f5f6f8}
@media(max-width:800px){main{padding:15px}.grid{grid-template-columns:repeat(2,1fr)}.wide{grid-column:span 2}.top{align-items:flex-start;flex-direction:column}}
</style></head><body><div class='layout'><main><div class='top'><div><h1>MIS_EXT Control</h1><div class='muted'>ESP8266 NAPT range extender</div></div><div id='state' class='pill'>Checking...</div></div>
<div class='grid'><div class='card metric'><span>Uplink signal</span><b id='rssi'>--</b></div><div class='card metric'><span>Connected clients</span><b id='clients'>--</b></div><div class='card metric'><span>Free heap</span><b id='heap'>--</b></div><div class='card metric'><span>Uptime</span><b id='uptime'>--</b></div>
<div class='card wide'><h2>Uplink signal history <span class='muted'>(dBm)</span></h2><canvas id='rssiChart' class='chart'></canvas><div class='muted'>Y-axis: signal strength in dBm. Closer to 0 is better.</div></div>
<div class='card wide'><h2>Free memory history <span class='muted'>(bytes)</span></h2><canvas id='heapChart' class='chart'></canvas><div class='muted'>Y-axis: free heap in bytes.</div></div>
<div class='card wide'><h2>Network details</h2><table><tr><th>Uplink</th><td id='sta'>--</td></tr><tr><th>Extender</th><td id='ap'>--</td></tr><tr><th>Channel</th><td id='channel'>--</td></tr><tr><th>Gateway</th><td id='gateway'>--</td></tr></table></div>
<div class='card wide'><h2>Internet usage</h2><p class='muted'>Per-client byte counters and throughput are not exposed by the ESP8266 NAPT API. This firmware does not display invented traffic values. Use the MIS router's traffic monitor for true internet usage.</p></div>
<div class='card wide'><h2>Connected devices</h2><table><thead><tr><th>#</th><th>MAC</th><th>IP</th><th>Connection history (last minute)</th></tr></thead><tbody id='devices'><tr><td colspan='4'>Loading...</td></tr></tbody></table><div class='muted'>Green indicates the device was connected when sampled. This is connection presence, not internet speed.</div></div>
<div class='card wide'><h2>Access</h2><div class='links'><a href='http://)rawliteral";
  s += WiFi.softAPIP().toString();
  s += R"rawliteral(/'>MIS_EXT dashboard</a><a href='http://)rawliteral";
  s += WiFi.localIP().toString();
  s += R"rawliteral(/'>MIS dashboard</a></div><p class='muted'>MIS_EXT / 88888888</p></div><div class='card wide'><h2>LED test</h2><p class='muted'>Run a short test without stopping the extender.</p><div class='links'><button onclick="testLed('uplink')">Test built-in LED</button><button onclick="testLed('event')">Test GPIO16 LED</button></div></div></div>
<script>const hist={r:[],h:[]};function $(x){return document.getElementById(x)}function fmt(s){return s<3600?Math.floor(s/60)+'m '+s%60+'s':Math.floor(s/3600)+'h '+Math.floor(s%3600/60)+'m'}
function draw(id,a,color){let c=$(id),x=c.getContext('2d'),d=devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;c.width=w*d;c.height=h*d;x.scale(d,d);x.clearRect(0,0,w,h);if(a.length<2)return;let lo=Math.min(...a),hi=Math.max(...a);if(hi==lo){hi++;lo--}let L=38,R=8,T=8,B=22,pw=w-L-R,ph=h-T-B;x.font='11px system-ui';x.fillStyle='#8fa2bd';x.fillText(Math.round(hi),2,T+5);x.fillText(Math.round(lo),2,h-B);x.fillText('0s',L,h-5);x.fillText(Math.max(0,(a.length-1)*3)+'s',w-30,h-5);x.strokeStyle='#263650';x.beginPath();x.moveTo(L,T);x.lineTo(L,h-B);x.lineTo(w-R,h-B);x.stroke();x.strokeStyle=color;x.lineWidth=2;x.beginPath();a.forEach((v,i)=>{let px=L+i*pw/(a.length-1),py=T+ph-(v-lo)*ph/(hi-lo);i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke()}
 let devHist={};function testLed(which){fetch('/api/led?which='+which).then(()=>alert('LED test started'))}function mini(){document.querySelectorAll('.devchart').forEach(c=>{let k=c.dataset.mac,a=devHist[k]||[];a.push(1);if(a.length>20)a.shift();devHist[k]=a;let x=c.getContext('2d'),w=c.width,h=c.height;x.clearRect(0,0,w,h);x.strokeStyle='#263650';x.beginPath();x.moveTo(0,h-1);x.lineTo(w,h-1);x.stroke();x.strokeStyle='#42d392';x.lineWidth=2;x.beginPath();a.forEach((v,i)=>{let p=i*w/19,y=h-5-v*(h-10);i?x.lineTo(p,y):x.moveTo(p,y)});x.stroke()})}
async function refresh(){try{let r=await fetch('/api/status',{cache:'no-store'}),j=await r.json();let ok=j.sta==3;$('state').textContent=ok?'UPLINK CONNECTED':'UPLINK OFFLINE';$('state').className='pill '+(ok?'up':'down');$('rssi').textContent=ok?j.rssi+' dBm':'offline';$('clients').textContent=j.clients;$('heap').textContent=j.heap+' B';$('uptime').textContent=fmt(j.uptime);$('sta').textContent=ok?j.staIp:'Disconnected';$('ap').textContent=j.apIp;$('channel').textContent=j.channel;$('gateway').textContent=j.gateway;hist.r.push(j.rssi);hist.h.push(j.heap);if(hist.r.length>60)hist.r.shift(),hist.h.shift();draw('rssiChart',hist.r,'#62a8ff');draw('heapChart',hist.h,'#42d392');$('devices').innerHTML=j.devices||'<tr><td colspan="4">No devices connected</td></tr>';mini()}catch(e){$('state').textContent='DASHBOARD OFFLINE';$('state').className='pill down'}}refresh();setInterval(refresh,3000);</script></main></body></html>)rawliteral";
  return s;
}

String statusJson() {
  String s = "{\"sta\":" + String((int)WiFi.status());
  s += ",\"staIp\":\"" + WiFi.localIP().toString() + "\",\"apIp\":\"" + WiFi.softAPIP().toString() + "\"";
  s += ",\"rssi\":" + String(WiFi.RSSI()) + ",\"channel\":" + String(WiFi.channel());
  s += ",\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",\"clients\":" + String(WiFi.softAPgetStationNum());
  s += ",\"heap\":" + String(ESP.getFreeHeap()) + ",\"uptime\":" + String(millis() / 1000);
  String rows = clientRows();
  rows.replace("'", "\\\"");
  s += ",\"devices\":\"" + rows + "\"}";
  return s;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(16, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // uplink status: blink until MIS connects
  digitalWrite(16, HIGH);         // client event LED: normally off
  Serial.begin(115200);
  Serial.println("\nMIS Extender + Web starting...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setOutputPower(20.5);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  // Avoid modem-sleep latency while the extender is actively forwarding.
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  loadCredentials();
  apSsid = staSsid.substring(0, 28) + "_EXT"; // keep the AP SSID within 32 bytes
  Serial.printf("Configured uplink: %s | AP: %s\n", staSsid.c_str(), apSsid.c_str());
  WiFi.begin(staSsid.c_str(), staPass.c_str());
  unsigned long t0 = millis();
  // Blink until connected to MIS (500ms toggle, server not started yet)
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Uplink OK: " + WiFi.localIP().toString());
    digitalWrite(LED_BUILTIN, HIGH); // off = connected
  } else {
    Serial.println("Uplink FAIL - continuing anyway (LED keeps blinking)");
  }
  auto& dhcp = WiFi.softAPDhcpServer();
  if (WiFi.status() == WL_CONNECTED) dhcp.setDns(WiFi.dnsIP(0));
  WiFi.mode(WIFI_AP_STA);
  int ch = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : 1;
  activeApChannel = ch;
  Serial.printf("STA CH:%d RSSI:%d -> starting AP on same CH\n", ch, WiFi.RSSI());
  WiFi.softAPConfig(IPAddress(192,168,5,1), IPAddress(192,168,5,1), IPAddress(255,255,255,0));
  bool apOk = WiFi.softAP(apSsid.c_str(), AP_PASS, ch, 0, 4);
  Serial.printf("softAP ok=%d CH=%d IP:", apOk, ch);
  Serial.println(WiFi.softAPIP().toString());
  Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
  err_t r = ip_napt_init(NAPT_ENTRIES, NAPT_PORTS);
  if (r == ERR_OK) { ip_napt_enable_no(SOFTAP_IF, 1); Serial.println("NAT on"); }
  server.on("/", []() { server.send(200, "text/html", page()); });
  server.on("/api/status", []() { server.send(200, "application/json", statusJson()); });
  server.on("/api/led", []() {
    if (server.arg("which") == "uplink") builtinTestUntil = millis() + 3000;
    if (server.arg("which") == "event") eventTestPulses = 6;
    server.send(200, "text/plain", "ok");
  });
  server.on("/setup", []() {
    String h = "<!doctype html><meta name='viewport' content='width=device-width,initial-scale=1'><title>WiFi Setup</title><style>body{font:16px Arial;max-width:480px;margin:40px auto;padding:20px}input{width:100%;padding:12px;margin:8px 0 16px;box-sizing:border-box}button{padding:12px 18px;background:#151a22;color:white;border:0;border-radius:4px}</style><h1>Uplink WiFi</h1><p>Current network: <b>" + staSsid + "</b></p><form method='POST' action='/save'><label>WiFi name (SSID)</label><input name='ssid' maxlength='64' required><label>Password</label><input name='pass' type='password' maxlength='64'><button>Save and reconnect</button></form><p>After saving, the extender name becomes <b>SSID_EXT</b>.</p>";
    server.send(200, "text/html", h);
  });
  server.on("/save", HTTP_POST, []() {
    if (!server.hasArg("ssid")) { server.send(400, "text/plain", "SSID required"); return; }
    saveCredentials(server.arg("ssid"), server.arg("pass"));
    server.send(200, "text/html", "<h1>Saved</h1><p>Rebooting and connecting to the new WiFi...</p>");
    delay(1000);
    ESP.restart();
  });
  server.begin();
  // ESP8266WebServer binds to 0.0.0.0 -> reachable on BOTH interfaces
  Serial.println("Web at http://192.168.5.1/ (via MIS_EXT)");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Web also at http://%s/ (via MIS)\n", WiFi.localIP().toString().c_str());
  }
}

void loop() {
  server.handleClient();
  // Recover the uplink without rebooting or interrupting the web server.
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt > 10000) {
    lastReconnectAttempt = millis();
    WiFi.disconnect();
    WiFi.begin(staSsid.c_str(), staPass.c_str());
  }
  // Built-in LED: blink while the uplink is down, off when connected.
  if (millis() < builtinTestUntil || WiFi.status() != WL_CONNECTED) {
    static unsigned long bt = 0;
    static bool on = false;
    if (millis() - bt > 250) {
      bt = millis();
      on = !on;
      digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
    }
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  // GPIO16: one blink on disconnect, two fast blinks on connect.
  static int previousClients = -1;
  int clients = WiFi.softAPgetStationNum();
  if (previousClients >= 0 && clients != previousClients) {
    eventTestPulses = clients > previousClients ? 4 : 2; // on/off pairs: 2 or 1
    eventTestOn = false;
    eventTestAt = 0;
  }
  previousClients = clients;
  if (eventTestPulses && millis() - eventTestAt >= 120) {
    eventTestAt = millis();
    eventTestOn = !eventTestOn;
    digitalWrite(16, eventTestOn ? LOW : HIGH);
    if (!eventTestOn) eventTestPulses--;
  }
  static unsigned long last = 0;
  if (millis() - last > 15000) {
    last = millis();
    Serial.printf("STA:%d IP:%s Clients:%d\n", WiFi.status(), WiFi.localIP().toString().c_str(), WiFi.softAPgetStationNum());
  }
}
