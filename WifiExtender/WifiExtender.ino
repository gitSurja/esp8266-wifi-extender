// NAPT Range extender - MIS -> MIS_EXT
#if LWIP_FEATURES && !LWIP_IPV6

#define STASSID "MIS"
#define STAPSK "CHANGE_ME"

#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>

#define NAPT 512
#define NAPT_PORT 16

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nNAPT Range extender MIS\n");
  Serial.printf("Heap on start: %d\n", ESP.getFreeHeap());

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.begin(STASSID, STAPSK);
  Serial.printf("Connecting to %s", STASSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 25000) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FAIL: could not connect to MIS. Check 2.4GHz, password, distance.");
    return;
  }
  Serial.printf("STA: %s (dns: %s / %s)\n", WiFi.localIP().toString().c_str(), WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str());

  auto& server = WiFi.softAPDhcpServer();
  server.setDns(WiFi.dnsIP(0));

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(
    IPAddress(172, 217, 28, 254), IPAddress(172, 217, 28, 254), IPAddress(255, 255, 255, 0));
  WiFi.softAP("MIS_EXT", "88888888");
  Serial.printf("AP: %s SSID:MIS_EXT\n", WiFi.softAPIP().toString().c_str());

  Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
  err_t ret = ip_napt_init(NAPT, NAPT_PORT);
  Serial.printf("ip_napt_init ret=%d (OK=%d)\n", (int)ret, (int)ERR_OK);
  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
    Serial.printf("ip_napt_enable ret=%d\n", (int)ret);
    if (ret == ERR_OK) { Serial.println(" extender ready: join MIS_EXT, password 88888888"); }
  }
  Serial.printf("Heap after: %d\n", ESP.getFreeHeap());
}

#else
void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nNAPT not supported\n");
}
#endif

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 10000) {
    last = millis();
    Serial.printf("STA:%d IP:%s Clients:%d\n", WiFi.status(), WiFi.localIP().toString().c_str(), WiFi.softAPgetStationNum());
  }
}
