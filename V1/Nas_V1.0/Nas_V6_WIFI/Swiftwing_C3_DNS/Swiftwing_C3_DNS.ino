/*
  Swiftwing NAS - ESP32-C3 DNS NODE
  Role:
    - local DNS override for swiftwingnas.tplinkdns.com
    - automatically learns the Master IP from UDP announcements
    - forwards all other DNS queries to Cloudflare 1.1.1.1
    - announces C3 health to the Master / Monitor

  Board: ESP32-C3
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <HTTPClient.h>

// =====================================================
// CONFIG
// =====================================================

const char* DEFAULT_WIFI_SSID     = "BaBaWu";
const char* DEFAULT_WIFI_PASSWORD = "95042700Wu$";

const char* NAS_DOMAIN    = "swiftwingnas.tplinkdns.com";

IPAddress UPSTREAM_DNS(1, 1, 1, 1);

// =====================================================
// Ports
// =====================================================

const uint16_t DNS_PORT              = 53;
const uint16_t UPSTREAM_LOCAL_PORT   = 53053;
const uint16_t MASTER_ANNOUNCE_PORT  = 4211;
const uint16_t MASTER_LOCAL_PORT     = 4212;
const uint16_t DNS_STATUS_PORT       = 4213;

const unsigned long MASTER_TIMEOUT_MS = 15000;
const unsigned long STATUS_INTERVAL_MS = 5000;
const unsigned long MASTER_FIND_INTERVAL_MS = 3000;
const unsigned long MASTER_SCAN_STEP_MS = 30;

WiFiUDP dnsUDP;
WiFiUDP upstreamUDP;
WiFiUDP masterUDP;
WiFiUDP statusUDP;


// =====================================================
// Swiftwing Wi-Fi sync / persistent configuration
// =====================================================

const char* SWIFTWING_SETUP_SSID = "Swiftwing-Bridge";
const char* SWIFTWING_SETUP_PASS = "swiftwing-setup";
const char* WIFI_SYNC_KEY         = "SWIFTWING_WIFI_V1";
const uint16_t WIFI_SYNC_PORT     = 4220;

Preferences wifiPrefs;
WiFiUDP wifiSyncUDP;

String savedWifiSSID = "";
String savedWifiPass = "";
unsigned long wifiRestartAt = 0;

String hexDecode(const String &hex) {
  String out;
  if (hex.length() % 2 != 0) return out;
  out.reserve(hex.length() / 2);

  auto hexVal = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };

  for (unsigned int i = 0; i < hex.length(); i += 2) {
    int hi = hexVal(hex[i]);
    int lo = hexVal(hex[i + 1]);
    if (hi < 0 || lo < 0) return "";
    out += char((hi << 4) | lo);
  }

  return out;
}

bool loadSavedWiFi() {
  wifiPrefs.begin("swiftwifi", true);
  savedWifiSSID = wifiPrefs.getString("ssid", "");
  savedWifiPass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();
  return savedWifiSSID.length() > 0;
}

void saveWiFiConfig(const String &ssid, const String &pass) {
  wifiPrefs.begin("swiftwifi", false);
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", pass);
  wifiPrefs.end();
  savedWifiSSID = ssid;
  savedWifiPass = pass;
}

bool waitForWiFi(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool fetchWiFiFromMasterBridge() {
  WiFiClient client;
  HTTPClient http;

  String url =
    String("http://192.168.4.1/api/wifi/bootstrap?key=") +
    WIFI_SYNC_KEY;

  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(2500);
  int code = http.GET();
  String body = code == 200 ? http.getString() : String("");
  http.end();

  if (code != 200 || !body.startsWith("SWIFTWING_BOOTSTRAP|")) {
    return false;
  }

  int p1 = body.indexOf('|');
  int p2 = body.indexOf('|', p1 + 1);

  if (p1 < 0 || p2 < 0) {
    return false;
  }

  String ssid = hexDecode(body.substring(p1 + 1, p2));
  String pass = hexDecode(body.substring(p2 + 1));

  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
    return false;
  }

  saveWiFiConfig(ssid, pass);

  Serial.print("Home WiFi received from Master: ");
  Serial.println(ssid);

  return true;
}

bool connectSavedHomeWiFi(unsigned long timeoutMs) {
  if (savedWifiSSID.length() == 0) return false;

  WiFi.disconnect();
  delay(150);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);

  Serial.print("Connecting home WiFi: ");
  Serial.println(savedWifiSSID);

  WiFi.begin(savedWifiSSID.c_str(), savedWifiPass.c_str());
  return waitForWiFi(timeoutMs);
}

void connectClusterWiFi() {
  WiFi.mode(WIFI_STA);
  // Minimum modem sleep keeps the station reachable while reducing idle RF power.
  WiFi.setSleep(true);

  bool connected = false;

  // Try saved WiFi first.
  if (loadSavedWiFi()) {
    connected = connectSavedHomeWiFi(8000);
  }

  // Built-in home WiFi fallback. This survives reflashing and stale NVS.
  if (!connected) {
    Serial.print("Trying built-in home WiFi: ");
    Serial.println(DEFAULT_WIFI_SSID);

    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);

    WiFi.begin(
      DEFAULT_WIFI_SSID,
      DEFAULT_WIFI_PASSWORD
    );

    connected = waitForWiFi(12000);

    if (connected) {
      saveWiFiConfig(
        DEFAULT_WIFI_SSID,
        DEFAULT_WIFI_PASSWORD
      );
    }
  }

  // If home WiFi is unavailable, use the Master bridge bootstrap path.
  while (!connected) {
    Serial.println("Home WiFi unavailable. Joining Swiftwing-Bridge for bootstrap...");

    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.begin(SWIFTWING_SETUP_SSID, SWIFTWING_SETUP_PASS);

    if (!waitForWiFi(15000)) {
      Serial.println("Swiftwing-Bridge not found. Retrying...");
      delay(1500);
      continue;
    }

    Serial.print("Bridge IP: ");
    Serial.println(WiFi.localIP());

    bool gotConfig = false;

    for (int attempt = 0; attempt < 60 && WiFi.status() == WL_CONNECTED; attempt++) {
      if (fetchWiFiFromMasterBridge()) {
        gotConfig = true;
        break;
      }
      delay(2000);
    }

    if (!gotConfig) {
      Serial.println("Master has no home WiFi yet. Reconnecting to Bridge...");
      delay(1000);
      continue;
    }

    connected = connectSavedHomeWiFi(15000);

    if (!connected) {
      Serial.println("Received WiFi but could not connect. Returning to Bridge...");
      delay(1000);
    }
  }

  Serial.print("WiFi connected: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void processWiFiSync() {
  int packetSize = wifiSyncUDP.parsePacket();

  while (packetSize) {
    char buffer[320];
    int len = wifiSyncUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);
    const String prefix = String("SWIFTWING_WIFI|") + WIFI_SYNC_KEY + "|";

    if (msg.startsWith(prefix)) {
      int split = msg.indexOf('|', prefix.length());
      if (split > 0) {
        String ssidHex = msg.substring(prefix.length(), split);
        String passHex = msg.substring(split + 1);
        String ssid = hexDecode(ssidHex);
        String pass = hexDecode(passHex);

        if (ssid.length() > 0 && ssid.length() <= 32 && pass.length() <= 64) {
          saveWiFiConfig(ssid, pass);

          Serial.print("New WiFi received from Master: ");
          Serial.println(ssid);

          wifiSyncUDP.beginPacket(wifiSyncUDP.remoteIP(), wifiSyncUDP.remotePort());
          wifiSyncUDP.print("SWIFTWING_WIFI_ACK|");
          wifiSyncUDP.print(WIFI_SYNC_KEY);
          wifiSyncUDP.endPacket();

          wifiRestartAt = millis() + 1200;
        }
      }
    }

    packetSize = wifiSyncUDP.parsePacket();
  }

  if (wifiRestartAt && (long)(millis() - wifiRestartAt) >= 0) {
    ESP.restart();
  }
}


IPAddress masterIP(0, 0, 0, 0);
bool masterKnown = false;
unsigned long masterLastSeen = 0;
unsigned long lastStatus = 0;
unsigned long lastMasterFind = 0;
unsigned long lastMasterScanStep = 0;
uint8_t masterScanHost = 1;
bool masterScanActive = false;

static uint8_t dnsBuffer[512];
static uint8_t upstreamBuffer[512];

// =====================================================
// Helpers
// =====================================================

String fieldAt(const String &s, int index) {
  int start = 0;
  int current = 0;

  while (true) {
    int end = s.indexOf('|', start);

    if (current == index) {
      if (end < 0) return s.substring(start);
      return s.substring(start, end);
    }

    if (end < 0) return "";
    start = end + 1;
    current++;
  }
}

bool parseIPv4(const String &text, IPAddress &result) {
  int a, b, c, d;

  if (sscanf(text.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;

  if (a < 0 || a > 255 || b < 0 || b > 255 ||
      c < 0 || c > 255 || d < 0 || d > 255) return false;

  result = IPAddress(a, b, c, d);
  return true;
}

bool parseQuestionName(
  const uint8_t *packet,
  int packetLen,
  int &offset,
  String &name
) {
  name = "";

  if (packetLen < 12) return false;

  offset = 12;

  while (offset < packetLen) {
    uint8_t labelLen = packet[offset++];

    if (labelLen == 0) break;
    if ((labelLen & 0xC0) != 0) return false;
    if (offset + labelLen > packetLen) return false;

    if (name.length()) name += ".";

    for (int i = 0; i < labelLen; i++) {
      name += (char)packet[offset++];
    }
  }

  return offset + 4 <= packetLen;
}

void sendEmptyReply(
  uint8_t *query,
  int queryLen,
  IPAddress clientIP,
  uint16_t clientPort,
  int questionEnd
) {
  if (questionEnd > queryLen || questionEnd < 12) return;

  query[2] = 0x81;
  query[3] = 0x80;

  query[6] = 0;
  query[7] = 0;
  query[8] = 0;
  query[9] = 0;
  query[10] = 0;
  query[11] = 0;

  dnsUDP.beginPacket(clientIP, clientPort);
  dnsUDP.write(query, questionEnd);
  dnsUDP.endPacket();
}

void sendLocalAReply(
  uint8_t *query,
  int queryLen,
  IPAddress clientIP,
  uint16_t clientPort,
  int questionEnd
) {
  if (questionEnd > queryLen || questionEnd < 12 || questionEnd + 16 > 512) return;

  // The caller returns after this reply, so its receive buffer can hold the answer.
  uint8_t *reply = query;
  int p = 0;
  p = questionEnd;

  reply[2] = 0x81;
  reply[3] = 0x80;

  // ANCOUNT = 1
  reply[6] = 0;
  reply[7] = 1;

  // NSCOUNT = 0, ARCOUNT = 0
  reply[8] = 0;
  reply[9] = 0;
  reply[10] = 0;
  reply[11] = 0;

  // Answer name pointer to question name at offset 12
  reply[p++] = 0xC0;
  reply[p++] = 0x0C;

  // TYPE A
  reply[p++] = 0x00;
  reply[p++] = 0x01;

  // CLASS IN
  reply[p++] = 0x00;
  reply[p++] = 0x01;

  // TTL 5 seconds
  reply[p++] = 0x00;
  reply[p++] = 0x00;
  reply[p++] = 0x00;
  reply[p++] = 0x05;

  // RDLENGTH = 4
  reply[p++] = 0x00;
  reply[p++] = 0x04;

  reply[p++] = masterIP[0];
  reply[p++] = masterIP[1];
  reply[p++] = masterIP[2];
  reply[p++] = masterIP[3];

  dnsUDP.beginPacket(clientIP, clientPort);
  dnsUDP.write(reply, p);
  dnsUDP.endPacket();
}

// =====================================================
// Master IP learning
// =====================================================

IPAddress subnetBroadcastIP() {
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();

  return IPAddress(
    ip[0] | (uint8_t)(~mask[0]),
    ip[1] | (uint8_t)(~mask[1]),
    ip[2] | (uint8_t)(~mask[2]),
    ip[3] | (uint8_t)(~mask[3])
  );
}

void sendMasterFindRequestTo(IPAddress targetIP) {
  String msg =
    "SWIFTWING_FIND_MASTER|C3|" +
    WiFi.localIP().toString();

  masterUDP.beginPacket(
    targetIP,
    MASTER_LOCAL_PORT
  );

  masterUDP.print(msg);
  masterUDP.endPacket();
}

void sendMasterFindRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  sendMasterFindRequestTo(
    IPAddress(255, 255, 255, 255)
  );

  IPAddress subnetBroadcast =
    subnetBroadcastIP();

  if (
    subnetBroadcast !=
    IPAddress(255, 255, 255, 255)
  ) {
    sendMasterFindRequestTo(
      subnetBroadcast
    );
  }
}


void startMasterUnicastScan() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  masterScanHost = 1;
  masterScanActive = true;
  lastMasterScanStep = 0;

  Serial.print("Scanning subnet for Master: ");
  Serial.print(WiFi.localIP()[0]);
  Serial.print(".");
  Serial.print(WiFi.localIP()[1]);
  Serial.print(".");
  Serial.print(WiFi.localIP()[2]);
  Serial.println(".1-254");
}

void processMasterUnicastScan() {
  if (
    !masterScanActive ||
    masterKnown ||
    WiFi.status() != WL_CONNECTED
  ) {
    return;
  }

  unsigned long now = millis();

  if (
    now - lastMasterScanStep <
    MASTER_SCAN_STEP_MS
  ) {
    return;
  }

  lastMasterScanStep = now;

  IPAddress localIP = WiFi.localIP();

  // Skip this C3's own address.
  if (masterScanHost != localIP[3]) {
    IPAddress target(
      localIP[0],
      localIP[1],
      localIP[2],
      masterScanHost
    );

    sendMasterFindRequestTo(target);
  }

  if (masterScanHost >= 254) {
    masterScanActive = false;
    lastMasterFind = now;

    Serial.println(
      "Master not found in this scan; will retry."
    );
  } else {
    masterScanHost++;
  }
}

void processMasterAnnouncements() {
  int packetSize = masterUDP.parsePacket();

  while (packetSize) {
    char buffer[160];

    int len = masterUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);

    // SWIFTWING_MASTER|ip|uptime
    if (msg.startsWith("SWIFTWING_MASTER|")) {
      IPAddress learned;

      if (parseIPv4(fieldAt(msg, 1), learned)) {
        bool changed = !masterKnown || learned != masterIP;

        masterIP = learned;
        masterKnown = true;
        masterLastSeen = millis();

        if (changed) {
          Serial.print("Master IP learned: ");
          Serial.println(masterIP);
        }
      }
    }

    packetSize = masterUDP.parsePacket();
  }

  if (masterKnown && millis() - masterLastSeen > MASTER_TIMEOUT_MS) {
    masterKnown = false;
    masterScanActive = false;
    startMasterUnicastScan();
    Serial.println("Master timeout");
  }
}

// =====================================================
// DNS proxy
// =====================================================

void forwardToUpstream(
  const uint8_t *query,
  int queryLen,
  IPAddress clientIP,
  uint16_t clientPort
) {
  upstreamUDP.beginPacket(UPSTREAM_DNS, 53);
  upstreamUDP.write(query, queryLen);
  upstreamUDP.endPacket();

  unsigned long start = millis();

  while (millis() - start < 900) {
    int n = upstreamUDP.parsePacket();

    if (n > 0) {
      int len = upstreamUDP.read(upstreamBuffer, sizeof(upstreamBuffer));

      dnsUDP.beginPacket(clientIP, clientPort);
      dnsUDP.write(upstreamBuffer, len);
      dnsUDP.endPacket();

      return;
    }

    // The wait can last up to 900 ms; poll less often to avoid busy looping.
    delay(5);
    yield();
  }
}

void processDNS() {
  int packetSize = dnsUDP.parsePacket();
  if (packetSize <= 0) return;

  IPAddress clientIP = dnsUDP.remoteIP();
  uint16_t clientPort = dnsUDP.remotePort();

  int len = dnsUDP.read(dnsBuffer, sizeof(dnsBuffer));
  if (len < 12) return;

  int offset = 0;
  String qname;

  if (!parseQuestionName(dnsBuffer, len, offset, qname)) return;

  int questionEnd = offset + 4;

  uint16_t qType = ((uint16_t)dnsBuffer[offset] << 8) | dnsBuffer[offset + 1];

  String lowerName = qname;
  lowerName.toLowerCase();

  String localName = String(NAS_DOMAIN);
  localName.toLowerCase();

  if (lowerName == localName) {
    if (qType == 1 && masterKnown) {
      sendLocalAReply(
        dnsBuffer,
        len,
        clientIP,
        clientPort,
        questionEnd
      );
    } else {
      // No AAAA record, or Master IP not known yet.
      sendEmptyReply(
        dnsBuffer,
        len,
        clientIP,
        clientPort,
        questionEnd
      );
    }

    return;
  }

  forwardToUpstream(dnsBuffer, len, clientIP, clientPort);
}

// =====================================================
// C3 health announcement
// =====================================================

void sendStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  String msg =
    "SWIFTWING_DNS|C3|" +
    WiFi.localIP().toString() +
    "|" +
    String(millis() / 1000UL);

  statusUDP.beginPacket(
    IPAddress(255, 255, 255, 255),
    DNS_STATUS_PORT
  );

  statusUDP.print(msg);
  statusUDP.endPacket();
}

// =====================================================
// Setup / loop
// =====================================================

void connectWiFi() {
  connectClusterWiFi();
}

void startUDP() {
  dnsUDP.begin(DNS_PORT);
  upstreamUDP.begin(UPSTREAM_LOCAL_PORT);
  masterUDP.begin(MASTER_ANNOUNCE_PORT);
  statusUDP.begin(DNS_STATUS_PORT + 100);
  wifiSyncUDP.begin(WIFI_SYNC_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  connectWiFi();
  startUDP();

  Serial.println("Swiftwing C3 DNS ONLINE");
  Serial.print(NAS_DOMAIN);
  Serial.println(" -> waiting for Master announcement");

  sendStatus();
  lastStatus = millis();

  sendMasterFindRequest();
  lastMasterFind = millis();

  startMasterUnicastScan();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    dnsUDP.stop();
    upstreamUDP.stop();
    masterUDP.stop();
    statusUDP.stop();
    wifiSyncUDP.stop();

    WiFi.disconnect();
    connectWiFi();
    startUDP();

    masterKnown = false;
  }

  processWiFiSync();
  processMasterAnnouncements();
  processDNS();

  processMasterUnicastScan();

  if (
    !masterKnown &&
    !masterScanActive &&
    millis() - lastMasterFind >= MASTER_FIND_INTERVAL_MS
  ) {
    // Keep broadcast as a quick try, but do not depend on it.
    sendMasterFindRequest();

    // Main discovery method: unicast scan of this /24 subnet.
    startMasterUnicastScan();

    lastMasterFind = millis();
  }

  if (millis() - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = millis();
    sendStatus();
  }

  delay(1);
}
