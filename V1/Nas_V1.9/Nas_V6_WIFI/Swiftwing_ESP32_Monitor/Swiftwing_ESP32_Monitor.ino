/*
  Swiftwing NAS - ESP32 MONITOR NODE / FOUR-WIRE FAN V1.2.5
  Role:
    - watches Master, Node 1, Node 2 and C3 DNS
    - reports monitor status back to Master
    - optional SSD1306 OLED dashboard
    - optional website-controlled 4-wire fan PWM input (open-collector driver required)

  Board: ESP32 Dev Module / ESP32-WROOM-32

  OLED is disabled by default so this compiles even before the screen
  and OLED libraries are installed.

  For a typical 0.96" I2C SSD1306:
    ENABLE_OLED -> 1
    SDA -> GPIO21
    SCL -> GPIO22

  Required libraries only when ENABLE_OLED = 1:
    Adafruit GFX
    Adafruit SSD1306
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <strings.h>
#include <Preferences.h>

// =====================================================
// CONFIG
// =====================================================

#define ENABLE_OLED 0
#define ENABLE_FAN_CONTROL 1
#define FAN_OUTPUT_4WIRE_PWM 1

// The Monitor only handles small UDP packets; 80 MHz keeps Wi-Fi and LEDC
// functional while reducing the ESP32's continuous CPU power draw.
const uint32_t MONITOR_CPU_FREQUENCY_MHZ = 80;

#if ENABLE_FAN_CONTROL
  // GPIO25 drives an external open-collector stage for a 4-wire fan PWM input.
  // The fan motor must be powered from a suitable 5 V supply, not this GPIO.
  // Set FAN_OUTPUT_4WIRE_PWM to 0 only when using the legacy 2/3-wire
  // low-side power MOSFET wiring documented in FAN_WIRING.txt.
  const uint8_t FAN_PWM_PIN = 25;
  const uint16_t FAN_CONTROL_PORT = 4216;
  const uint32_t FAN_PWM_FREQUENCY_HZ = 25000;
  const uint8_t FAN_PWM_RESOLUTION_BITS = 8;
  const unsigned long FAN_START_KICK_MS = 300;

  Preferences fanPreferences;
  WiFiUDP fanControlUDP;
  bool fanPwmReady = false;
  uint8_t fanTargetPercent = 0;
  uint8_t fanOutputPercent = 0;
#endif

#if ENABLE_OLED
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  const int OLED_SDA = 21;
  const int OLED_SCL = 22;

  Adafruit_SSD1306 display(128, 64, &Wire, -1);
  bool oledReady = false;
#endif

// =====================================================
// Protocol
// =====================================================

const uint16_t STORAGE_DISCOVERY_PORT = 4210;
const uint16_t MONITOR_LOCAL_PORT     = 4215;
const uint16_t MASTER_ANNOUNCE_PORT   = 4211;
const uint16_t DNS_STATUS_PORT        = 4213;
const uint16_t MONITOR_STATUS_PORT    = 4214;

const unsigned long DISCOVERY_INTERVAL_MS = 4000;
const unsigned long STATUS_INTERVAL_MS    = 5000;
const unsigned long DEVICE_TIMEOUT_MS     = 15000;
const unsigned long SCREEN_INTERVAL_MS    = 3000;

WiFiUDP discoveryUDP;
WiFiUDP masterUDP;
WiFiUDP dnsUDP;
WiFiUDP statusUDP;


// =====================================================
// Swiftwing Wi-Fi sync / persistent configuration
// =====================================================

const char* SWIFTWING_SETUP_SSID = "Swiftwing-Bridge";
const char* SWIFTWING_SETUP_PASS = "swiftwing-setup";
const char* WIFI_SYNC_KEY         = "SWIFTWING_WIFI_V1";
const uint16_t WIFI_SYNC_PORT     = 4220;
const uint32_t WIFI_EEPROM_MAGIC  = 0x53574631UL;

WiFiUDP wifiSyncUDP;
unsigned long wifiRestartAt = 0;

struct StoredWiFiRecord {
  uint32_t magic;
  char ssid[33];
  char pass[65];
};

StoredWiFiRecord wifiRecord;

String hexDecode(const String &hex) {
  String out;
  if (hex.length() % 2 != 0) return out;
  out.reserve(hex.length() / 2);

  for (unsigned int i = 0; i < hex.length(); i += 2) {
    auto val = [](char c)->int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int hi = val(hex[i]);
    int lo = val(hex[i+1]);
    if (hi < 0 || lo < 0) return "";
    out += char((hi << 4) | lo);
  }
  return out;
}

bool loadSavedWiFi() {
  EEPROM.begin(160);
  EEPROM.get(0, wifiRecord);
  wifiRecord.ssid[32] = '\0';
  wifiRecord.pass[64] = '\0';
  return wifiRecord.magic == WIFI_EEPROM_MAGIC && strlen(wifiRecord.ssid) > 0;
}

void saveWiFiConfig(const String &ssid, const String &pass) {
  memset(&wifiRecord, 0, sizeof(wifiRecord));
  wifiRecord.magic = WIFI_EEPROM_MAGIC;
  strncpy(wifiRecord.ssid, ssid.c_str(), sizeof(wifiRecord.ssid)-1);
  strncpy(wifiRecord.pass, pass.c_str(), sizeof(wifiRecord.pass)-1);
  EEPROM.put(0, wifiRecord);
  EEPROM.commit();
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
  client.setTimeout(2500);
  if (!client.connect(IPAddress(192, 168, 4, 1), 80, 2500)) return false;
  client.print("GET /api/wifi/bootstrap?key=");
  client.print(WIFI_SYNC_KEY);
  client.print(" HTTP/1.1\r\nHost: 192.168.4.1\r\nConnection: close\r\n\r\n");

  char line[256];
  size_t n = client.readBytesUntil('\n', line, sizeof(line) - 1);
  if (n < 13 || n >= sizeof(line) - 1) { client.stop(); return false; }
  line[n] = '\0';
  if (strncmp(line, "HTTP/1.", 7) != 0 || line[8] != ' ' ||
      strncmp(line + 9, "200", 3) != 0 || line[12] != ' ') {
    client.stop();
    return false;
  }

  int bodyLength = -1;
  bool headersDone = false;
  for (int header = 0; header < 16; ++header) {
    n = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (n == 0 || n >= sizeof(line) - 1) { client.stop(); return false; }
    line[n] = '\0';
    if (n == 1 && line[0] == '\r') { headersDone = true; break; }
    if (strncasecmp(line, "Content-Length:", 15) != 0) continue;
    const char *value = line + 15;
    while (*value == ' ' || *value == '\t') ++value;
    int length = 0;
    bool hasDigit = false;
    while (*value >= '0' && *value <= '9') {
      hasDigit = true;
      length = length * 10 + (*value++ - '0');
      if (length >= 256) { client.stop(); return false; }
    }
    if (!hasDigit) { client.stop(); return false; }
    bodyLength = length;
  }
  if (!headersDone || bodyLength <= 0) { client.stop(); return false; }

  char bodyBytes[256];
  if (client.readBytes(bodyBytes, bodyLength) != (size_t)bodyLength) {
    client.stop();
    return false;
  }
  bodyBytes[bodyLength] = '\0';
  client.stop();
  String body(bodyBytes);
  if (!body.startsWith("SWIFTWING_BOOTSTRAP|")) {
    return false;
  }

  int p1 = body.indexOf('|');
  int p2 = body.indexOf('|', p1 + 1);

  if (p1 < 0 || p2 < 0) return false;

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
  if (wifiRecord.magic != WIFI_EEPROM_MAGIC || strlen(wifiRecord.ssid) == 0) {
    return false;
  }

  WiFi.disconnect();
  delay(150);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.begin(wifiRecord.ssid, wifiRecord.pass);
  return waitForWiFi(timeoutMs);
}

void connectClusterWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);

  bool connected = false;

  if (loadSavedWiFi()) {
    Serial.print("Connecting saved WiFi: ");
    Serial.println(wifiRecord.ssid);
    connected = connectSavedHomeWiFi(12000);
  }

  if (!connected && (wifiRecord.magic != WIFI_EEPROM_MAGIC || strlen(wifiRecord.ssid) == 0)) {
    Serial.println("Trying previous WiFi credentials...");
    WiFi.begin();
    connected = waitForWiFi(8000);
  }

  while (!connected) {
    Serial.println("Home WiFi unavailable. Joining Swiftwing-Bridge for bootstrap...");

    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.begin(SWIFTWING_SETUP_SSID, SWIFTWING_SETUP_PASS);

    if (!waitForWiFi(15000)) {
      Serial.println("Swiftwing-Bridge not found. Retrying...");
      // Stop scanning during the retry pause; an unavailable router should
      // not keep the radio at its highest continuous power draw.
      WiFi.mode(WIFI_OFF);
      delay(5000);
      continue;
    }

    // The Master may take minutes to receive home Wi-Fi settings. Keep the
    // radio in modem sleep while waiting for its bootstrap endpoint.
    WiFi.setSleep(true);

    bool gotConfig = false;

    for (int attempt = 0; attempt < 60 && WiFi.status() == WL_CONNECTED; attempt++) {
      if (fetchWiFiFromMasterBridge()) {
        gotConfig = true;
        break;
      }
      delay(2000);
    }

    if (!gotConfig) {
      Serial.println("Master has no home WiFi yet. Retrying Bridge...");
      delay(1000);
      continue;
    }

    // Reload the record written to EEPROM and move to the real home LAN.
    loadSavedWiFi();
    connected = connectSavedHomeWiFi(15000);

    if (!connected) {
      Serial.println("Received WiFi but could not connect. Returning to Bridge...");
      delay(1000);
    }
  }

  // Keep the monitor in minimum modem sleep while it waits for status packets.
  WiFi.setSleep(true);

  Serial.print("Monitor WiFi: ");
  Serial.println(WiFi.SSID());
  Serial.print("Monitor IP: ");
  Serial.println(WiFi.localIP());
}

void processWiFiSync() {
  int packetSize = wifiSyncUDP.parsePacket();
  while (packetSize) {
    char buffer[320];
    int len = wifiSyncUDP.read(buffer, sizeof(buffer)-1);
    if (len > 0) buffer[len] = '\0';
    String msg(buffer);
    const String prefix = String("SWIFTWING_WIFI|") + WIFI_SYNC_KEY + "|";

    if (msg.startsWith(prefix)) {
      int split = msg.indexOf('|', prefix.length());
      if (split > 0) {
        String ssid = hexDecode(msg.substring(prefix.length(), split));
        String pass = hexDecode(msg.substring(split+1));
        if (ssid.length() > 0 && ssid.length() <= 32 && pass.length() <= 64) {
          saveWiFiConfig(ssid, pass);
          Serial.print("New WiFi received from Master: ");
          Serial.println(ssid);
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


bool masterOnline = false;
IPAddress masterIP;
unsigned long masterLastSeen = 0;

bool node1Online = false;
IPAddress node1IP;
uint64_t node1Total = 0;
uint64_t node1Free = 0;
unsigned long node1LastSeen = 0;

bool node2Online = false;
IPAddress node2IP;
uint64_t node2Total = 0;
uint64_t node2Free = 0;
unsigned long node2LastSeen = 0;

bool dnsOnline = false;
IPAddress dnsIP;
unsigned long dnsLastSeen = 0;

unsigned long lastDiscovery = 0;
unsigned long lastStatus = 0;
unsigned long lastScreen = 0;
int screenPage = 0;

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

String shortGB(uint64_t bytes) {
  if (bytes == 0) return "--";
  return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + "G";
}

// =====================================================
// Fan PWM control
// =====================================================

#if ENABLE_FAN_CONTROL

uint8_t fanDutyFromPercent(uint8_t percent) {
#if FAN_OUTPUT_4WIRE_PWM
  // GPIO25 drives an NPN/N-MOS open-collector stage, which inverts the signal:
  // a high GPIO pulls the fan PWM input low. The fan expects 100% high for max.
  uint8_t pinPercent = 100U - percent;
#else
  uint8_t pinPercent = percent;
#endif
  return (uint8_t)(((uint16_t)pinPercent * 255U + 50U) / 100U);
}

void writeFanOutput() {
  if (fanPwmReady) {
    ledcWrite(FAN_PWM_PIN, fanDutyFromPercent(fanOutputPercent));
  }
}

void setFanPercent(uint8_t percent, bool persist) {
  if (percent > 100) percent = 100;
  bool changed = percent != fanTargetPercent;
  bool startKick = percent > 0 && fanTargetPercent == 0;
  fanTargetPercent = percent;

  if (startKick) {
    // Finish the start pulse here. setup() and Wi-Fi reconnect can block for
    // minutes, so a loop-driven timer could leave the fan at full speed.
    fanOutputPercent = 100;
    writeFanOutput();
    delay(FAN_START_KICK_MS);
  }

  fanOutputPercent = percent;
  writeFanOutput();

  // Persist only actual changes, so repeated UDP retries do not wear flash.
  if (persist && changed) {
    fanPreferences.putUChar("percent", fanTargetPercent);
  }
}

void setupFanControl() {
  fanPreferences.begin("swfan", false);
  uint8_t savedPercent = fanPreferences.getUChar("percent", 0);
  if (savedPercent > 100) savedPercent = 0;

  fanPwmReady = ledcAttach(
    FAN_PWM_PIN,
    FAN_PWM_FREQUENCY_HZ,
    FAN_PWM_RESOLUTION_BITS
  );

  if (!fanPwmReady) {
    Serial.println("Fan PWM could not attach to GPIO25");
    return;
  }

  setFanPercent(savedPercent, false);
}

void processFanControl() {
  int packetSize = fanControlUDP.parsePacket();

  while (packetSize) {
    char buffer[48];
    int len = fanControlUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) {
      buffer[len] = '\0';

      // Accept control packets only from the currently announced Master.
      unsigned int percent = 0;
      char trailing = '\0';
      if (
        masterOnline &&
        fanControlUDP.remoteIP() == masterIP &&
        sscanf(buffer, "SWIFTWING_FAN|V1|%u%c", &percent, &trailing) == 1 &&
        percent <= 100 && fanPwmReady
      ) {
        setFanPercent((uint8_t)percent, true);
      }
    }

    packetSize = fanControlUDP.parsePacket();
  }
}

#endif

// =====================================================
// Master announcements
// =====================================================

void processMaster() {
  int packetSize = masterUDP.parsePacket();

  while (packetSize) {
    char buffer[160];
    int len = masterUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);

    if (msg.startsWith("SWIFTWING_MASTER|")) {
      IPAddress ip;

      if (parseIPv4(fieldAt(msg, 1), ip)) masterIP = ip;
      else masterIP = masterUDP.remoteIP();

      masterOnline = true;
      masterLastSeen = millis();
    }

    packetSize = masterUDP.parsePacket();
  }
}

// =====================================================
// Storage discovery
// =====================================================

void sendDiscovery() {
  discoveryUDP.beginPacket(
    IPAddress(255, 255, 255, 255),
    STORAGE_DISCOVERY_PORT
  );

  discoveryUDP.print("SWIFTWING_DISCOVER|MONITOR");
  discoveryUDP.endPacket();
}

void processStorageReplies() {
  int packetSize = discoveryUDP.parsePacket();

  while (packetSize) {
    char buffer[256];
    int len = discoveryUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);

    // SWIFTWING_STORAGE|NODE1|ip|port|ready|total|used|free|uptime
    if (msg.startsWith("SWIFTWING_STORAGE|")) {
      String id = fieldAt(msg, 1);
      IPAddress ip;

      if (!parseIPv4(fieldAt(msg, 2), ip)) ip = discoveryUDP.remoteIP();

      uint64_t total = strtoull(fieldAt(msg, 5).c_str(), nullptr, 10);
      uint64_t freeB = strtoull(fieldAt(msg, 7).c_str(), nullptr, 10);

      if (id == "NODE1") {
        node1IP = ip;
        node1Total = total;
        node1Free = freeB;
        node1Online = true;
        node1LastSeen = millis();
      }

      if (id == "NODE2") {
        node2IP = ip;
        node2Total = total;
        node2Free = freeB;
        node2Online = true;
        node2LastSeen = millis();
      }
    }

    packetSize = discoveryUDP.parsePacket();
  }
}

// =====================================================
// DNS status
// =====================================================

void processDNS() {
  int packetSize = dnsUDP.parsePacket();

  while (packetSize) {
    char buffer[160];
    int len = dnsUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);

    if (msg.startsWith("SWIFTWING_DNS|C3|")) {
      IPAddress ip;

      if (parseIPv4(fieldAt(msg, 2), ip)) dnsIP = ip;
      else dnsIP = dnsUDP.remoteIP();

      dnsOnline = true;
      dnsLastSeen = millis();
    }

    packetSize = dnsUDP.parsePacket();
  }
}

// =====================================================
// Status to Master
// =====================================================

void sendMonitorStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  String msg =
    "SWIFTWING_MONITOR|ESP32|" +
    WiFi.localIP().toString() +
    "|" +
    String(millis() / 1000UL) +
    "|" +
    String(WiFi.RSSI());

#if ENABLE_FAN_CONTROL
  msg += fanPwmReady ? "|FAN1|" : "|FAN0|";
  msg += String(fanPwmReady ? fanTargetPercent : 0);
  msg += "|";
  msg += String(fanPwmReady ? fanOutputPercent : 0);
#else
  msg += "|FAN0|0|0";
#endif

  statusUDP.beginPacket(
    IPAddress(255, 255, 255, 255),
    MONITOR_STATUS_PORT
  );

  statusUDP.print(msg);
  statusUDP.endPacket();
}

// =====================================================
// OLED
// =====================================================

#if ENABLE_OLED

void drawStatusLine(int y, const char* name, bool online) {
  display.setCursor(0, y);
  display.print(name);
  display.setCursor(84, y);
  display.print(online ? "ONLINE" : "OFF");
}

void updateOLED() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (screenPage == 0) {
    display.setCursor(0, 0);
    display.println("SWIFTWING NAS");

    drawStatusLine(14, "Master", masterOnline);
    drawStatusLine(26, "Node 1", node1Online);
    drawStatusLine(38, "Node 2", node2Online);
    drawStatusLine(50, "DNS C3", dnsOnline);
  }

  else if (screenPage == 1) {
    display.setCursor(0, 0);
    display.println("STORAGE");

    display.setCursor(0, 16);
    display.print("N1 total ");
    display.println(shortGB(node1Total));

    display.setCursor(0, 28);
    display.print("N1 free  ");
    display.println(shortGB(node1Free));

    display.setCursor(0, 42);
    display.print("N2 total ");
    display.println(shortGB(node2Total));

    display.setCursor(0, 54);
    display.print("N2 free  ");
    display.println(shortGB(node2Free));
  }

  else {
    display.setCursor(0, 0);
    display.println("MONITOR");

    display.setCursor(0, 18);
    display.print("WiFi ");
    display.print(WiFi.RSSI());
    display.println(" dBm");

    display.setCursor(0, 32);
    display.print("IP ");
    display.println(WiFi.localIP());

    display.setCursor(0, 46);
    display.print("Up ");
    display.print(millis() / 1000UL);
    display.println(" sec");
  }

  display.display();
}

#endif

// =====================================================
// Timeouts
// =====================================================

void updateTimeouts() {
  unsigned long now = millis();

  if (masterOnline && now - masterLastSeen > DEVICE_TIMEOUT_MS) masterOnline = false;
  if (node1Online && now - node1LastSeen > DEVICE_TIMEOUT_MS) node1Online = false;
  if (node2Online && now - node2LastSeen > DEVICE_TIMEOUT_MS) node2Online = false;
  if (dnsOnline && now - dnsLastSeen > DEVICE_TIMEOUT_MS) dnsOnline = false;
}

// =====================================================
// Setup / loop
// =====================================================

void connectWiFi() {
  connectClusterWiFi();
}

void startUDP() {
  discoveryUDP.begin(MONITOR_LOCAL_PORT);
  masterUDP.begin(MASTER_ANNOUNCE_PORT);
  dnsUDP.begin(DNS_STATUS_PORT);
  statusUDP.begin(MONITOR_STATUS_PORT + 100);
  wifiSyncUDP.begin(WIFI_SYNC_PORT);
#if ENABLE_FAN_CONTROL
  fanControlUDP.begin(FAN_CONTROL_PORT);
#endif
}

void setup() {
  Serial.begin(115200);

  if (!setCpuFrequencyMhz(MONITOR_CPU_FREQUENCY_MHZ)) {
    Serial.println("Monitor CPU frequency reduction unavailable");
  }

  delay(100);

#if ENABLE_FAN_CONTROL
  // Apply the saved fan setting before Wi-Fi startup can block or retry.
  setupFanControl();
#endif

  connectWiFi();
  startUDP();

#if ENABLE_OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledReady = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Swiftwing Monitor");
    display.display();
  }
#endif

  sendDiscovery();
  sendMonitorStatus();

  lastDiscovery = millis();
  lastStatus = millis();
  lastScreen = millis();

  Serial.println("Swiftwing ESP32 Monitor ONLINE");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    discoveryUDP.stop();
    masterUDP.stop();
    dnsUDP.stop();
    statusUDP.stop();
    wifiSyncUDP.stop();
#if ENABLE_FAN_CONTROL
    fanControlUDP.stop();
#endif

    WiFi.disconnect();
    connectWiFi();
    startUDP();
  }

  processWiFiSync();
  processMaster();
  processStorageReplies();
  processDNS();
#if ENABLE_FAN_CONTROL
  processFanControl();
#endif
  updateTimeouts();

  unsigned long now = millis();

  if (now - lastDiscovery >= DISCOVERY_INTERVAL_MS) {
    lastDiscovery = now;
    sendDiscovery();
  }

  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    sendMonitorStatus();
  }

#if ENABLE_OLED
  if (now - lastScreen >= SCREEN_INTERVAL_MS) {
    lastScreen = now;
    screenPage = (screenPage + 1) % 3;
    updateOLED();
  }
#endif

  // Keep UDP controls responsive without waking the Arduino task 100 times/s.
  delay(50);
}
