/*
  Swiftwing NAS - STORAGE NODE 1
  Board: ESP32
  Storage: SunFounder Camera Extension Board microSD slot.
  Camera is NOT used.

  This node uses SD_MMC. It is intentionally separate from the Master.
*/

#include "SD_MMC.h"

const char* NODE_ID    = "NODE1";
const char* NODE_LABEL = "Storage Node 1";

// Defined in the shared node implementation below.
extern bool storageReady;

bool beginStorage() {
  // Try 4-bit SDMMC first for better throughput.
  if (SD_MMC.begin("/sdcard", false)) {
    return true;
  }

  SD_MMC.end();
  delay(100);

  // Fallback: 1-bit mode.
  return SD_MMC.begin("/sdcard", true);
}

fs::FS& store() {
  return SD_MMC;
}

uint64_t storageTotal() {
  return storageReady ? SD_MMC.totalBytes() : 0;
}

uint64_t storageUsed() {
  return storageReady ? SD_MMC.usedBytes() : 0;
}


#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "FS.h"
#include <strings.h>

// =====================================================
// CONFIG
// =====================================================

// Must match Master and the other storage node.
const char* NODE_API_KEY  = "BaBaWu";

const uint16_t HTTP_PORT              = 80;
const uint16_t STORAGE_DISCOVERY_PORT = 4210;

WebServer server(HTTP_PORT);
WiFiUDP discoveryUDP;


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
  WiFi.setSleep(false);

  Serial.print("Connecting home WiFi: ");
  Serial.println(savedWifiSSID);

  WiFi.begin(savedWifiSSID.c_str(), savedWifiPass.c_str());
  return waitForWiFi(timeoutMs);
}

void connectClusterWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  bool connected = false;

  if (loadSavedWiFi()) {
    connected = connectSavedHomeWiFi(12000);
  }

  if (!connected && savedWifiSSID.length() == 0) {
    // Migration path from old V6 firmware.
    Serial.println("Trying previous V6 WiFi credentials...");
    WiFi.begin();
    connected = waitForWiFi(8000);

    if (connected) {
      Serial.println("Previous WiFi connection restored.");
    }
  }

  while (!connected) {
    Serial.println("Home WiFi unavailable. Joining Swiftwing-Bridge for bootstrap...");

    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(SWIFTWING_SETUP_SSID, SWIFTWING_SETUP_PASS);

    if (!waitForWiFi(15000)) {
      Serial.println("Swiftwing-Bridge not found. Retrying...");
      // Avoid keeping the radio in scan mode throughout an outage.
      WiFi.mode(WIFI_OFF);
      delay(5000);
      continue;
    }

    Serial.print("Bridge IP: ");
    Serial.println(WiFi.localIP());

    // Bootstrap polling is idle traffic; home-Wi-Fi connection wakes the
    // radio again before the next association attempt.
    WiFi.setSleep(true);

    bool gotConfig = false;

    // Stay on the bridge and poll Master until it has a home Wi-Fi config.
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
  // Minimum modem sleep lowers idle radio power. Transfers wake it below.
  WiFi.setSleep(true);
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


bool storageReady = false;

File uploadFile;
bool uploadAccepted = false;
bool uploadComplete = false;
int uploadFinalStatus = 200;
String uploadFinalMessage = "OK";

bool rawUploadStarted = false;
size_t rawUploadExpected = 0;
size_t rawUploadReceived = 0;

// WebServer handles one request at a time, so upload and download can share RAM.
static uint8_t transferBuffer[8192];
size_t uploadBuffered = 0;
bool transferRadioAwake = false;
bool transferInProgress = false;
unsigned long transferEndedAt = 0;
const unsigned long TRANSFER_WIFI_AWAKE_MS = 2000;
unsigned long lastTransferDiscoveryAt = 0;
const unsigned long TRANSFER_DISCOVERY_INTERVAL_MS = 500;

void beginTransfer() {
  if (!transferRadioAwake) {
    WiFi.setSleep(false);
    transferRadioAwake = true;
  }
  transferInProgress = true;
  lastTransferDiscoveryAt = millis() - TRANSFER_DISCOVERY_INTERVAL_MS;
}

void endTransfer() {
  transferInProgress = false;
  transferEndedAt = millis();
}

void updateTransferPower() {
  if (transferRadioAwake && !transferInProgress &&
      millis() - transferEndedAt >= TRANSFER_WIFI_AWAKE_MS) {
    WiFi.setSleep(true);
    transferRadioAwake = false;
  }
}

// =====================================================
// Utility
// =====================================================

String normalizePath(String path) {
  path.trim();

  if (path.length() == 0) path = "/";
  if (!path.startsWith("/")) path = "/" + path;

  while (path.indexOf("//") >= 0) path.replace("//", "/");

  if (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }

  if (path.indexOf("..") >= 0) return "/";

  return path;
}

String fileNameOnly(String path) {
  int i = path.lastIndexOf('/');
  if (i < 0) return path;
  return path.substring(i + 1);
}

String parentPath(String path) {
  path = normalizePath(path);
  if (path == "/") return "/";

  int i = path.lastIndexOf('/');
  if (i <= 0) return "/";

  return path.substring(0, i);
}

String joinPath(String folder, String name) {
  folder = normalizePath(folder);
  if (folder == "/") return "/" + name;
  return folder + "/" + name;
}

bool validSimpleName(String name) {
  name.trim();

  if (name.length() == 0) return false;
  if (name.indexOf("/") >= 0) return false;
  if (name.indexOf("\\") >= 0) return false;
  if (name.indexOf("..") >= 0) return false;

  return true;
}

bool hiddenName(const String &name) {
  return (
    name == "System Volume Information" ||
    name == "$RECYCLE.BIN" ||
    name == ".Trashes" ||
    name == ".Spotlight-V100" ||
    name == ".fseventsd"
  );
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\r", "\\r");
  s.replace("\n", "\\n");
  return s;
}

String extensionOf(String name) {
  int dot = name.lastIndexOf('.');
  if (dot < 0 || dot == name.length() - 1) return "";
  String ext = name.substring(dot + 1);
  ext.toUpperCase();
  return ext;
}

String contentType(String name) {
  String lower = name;
  lower.toLowerCase();

  if (lower.endsWith(".pdf")) return "application/pdf";
  if (lower.endsWith(".txt")) return "text/plain";
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
  if (lower.endsWith(".png")) return "image/png";
  if (lower.endsWith(".gif")) return "image/gif";
  if (lower.endsWith(".zip")) return "application/zip";
  if (lower.endsWith(".mp3")) return "audio/mpeg";
  if (lower.endsWith(".mp4")) return "video/mp4";
  if (lower.endsWith(".json")) return "application/json";
  if (lower.endsWith(".csv")) return "text/csv";

  return "application/octet-stream";
}

bool authorized() {
  return server.hasArg("key") && server.arg("key") == NODE_API_KEY;
}

void cors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Swiftwing-Key, X-Swiftwing-Path, X-Swiftwing-Name");
  server.sendHeader("Access-Control-Max-Age", "600");
  server.sendHeader("Cache-Control", "no-store");
}

void sendText(int code, const String &text) {
  cors();
  server.send(code, "text/plain; charset=utf-8", text);
}

void sendJson(int code, const String &json) {
  cors();
  server.send(code, "application/json; charset=utf-8", json);
}

bool requireAPI() {
  if (!authorized()) {
    sendText(401, "Unauthorized");
    return false;
  }

  if (!storageReady) {
    sendText(503, "Storage not ready");
    return false;
  }

  return true;
}

// =====================================================
// Storage-specific functions supplied per node
// =====================================================

// bool beginStorage()
// fs::FS& store()
// uint64_t storageTotal()
// uint64_t storageUsed()

// =====================================================
// Node discovery
// =====================================================

void sendDiscoveryReply(IPAddress remoteIP, uint16_t remotePort) {
  const uint64_t total = storageTotal();
  const uint64_t used = storageUsed();
  const uint64_t freeBytes = total >= used ? total - used : 0;
  String response =
    String("SWIFTWING_STORAGE|") +
    NODE_ID +
    "|" +
    WiFi.localIP().toString() +
    "|" +
    String(HTTP_PORT) +
    "|" +
    String(storageReady ? 1 : 0) +
    "|" +
    String((unsigned long long)total) +
    "|" +
    String((unsigned long long)used) +
    "|" +
    String((unsigned long long)freeBytes) +
    "|" +
    String(millis() / 1000UL);

  discoveryUDP.beginPacket(remoteIP, remotePort);
  discoveryUDP.print(response);
  discoveryUDP.endPacket();
}

void processDiscovery() {
  int packetSize = discoveryUDP.parsePacket();

  while (packetSize) {
    char buffer[160];

    int len = discoveryUDP.read(buffer, sizeof(buffer) - 1);
    if (len > 0) buffer[len] = '\0';

    String msg(buffer);

    if (msg.startsWith("SWIFTWING_DISCOVER|")) {
      sendDiscoveryReply(discoveryUDP.remoteIP(), discoveryUDP.remotePort());
    }

    packetSize = discoveryUDP.parsePacket();
  }
}

void serviceTransferDiscovery() {
  unsigned long now = millis();
  if (now - lastTransferDiscoveryAt >= TRANSFER_DISCOVERY_INTERVAL_MS) {
    lastTransferDiscoveryAt = now;
    processDiscovery();
  }
}

// =====================================================
// File listing + search
// =====================================================

String itemJson(const String &path, const String &name, bool directory, uint64_t size) {
  String s = "{";
  s += "\"name\":\"" + jsonEscape(name) + "\",";
  s += "\"path\":\"" + jsonEscape(path) + "\",";
  s += "\"directory\":" + String(directory ? "true" : "false") + ",";
  s += "\"size\":" + String((unsigned long long)size) + ",";
  s += "\"ext\":\"" + jsonEscape(directory ? "" : extensionOf(name)) + "\"";
  s += "}";
  return s;
}

void handleList() {
  if (!requireAPI()) return;

  String path = server.hasArg("path") ? normalizePath(server.arg("path")) : "/";

  File dir = store().open(path);

  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    sendText(404, "Folder not found");
    return;
  }

  String folders = "";
  String files = "";
  int folderCount = 0;
  int fileCount = 0;
  int totalCount = 0;
  bool truncated = false;

  File item = dir.openNextFile();

  while (item) {
    if (totalCount >= 250) {
      truncated = true;
      item.close();
      break;
    }

    String fullPath = String(item.path());
    String name = fileNameOnly(fullPath);

    if (!hiddenName(name)) {
      bool isDir = item.isDirectory();
      uint64_t size = isDir ? 0 : item.size();

      String entry = itemJson(fullPath, name, isDir, size);

      if (isDir) {
        if (folderCount++) folders += ",";
        folders += entry;
      } else {
        if (fileCount++) files += ",";
        files += entry;
      }

      totalCount++;
    }

    item.close();
    item = dir.openNextFile();
  }

  dir.close();

  String json = "{";
  json += "\"node\":\"" + String(NODE_ID) + "\",";
  json += "\"path\":\"" + jsonEscape(path) + "\",";
  json += "\"truncated\":" + String(truncated ? "true" : "false") + ",";
  json += "\"items\":[" + folders;
  if (folderCount && fileCount) json += ",";
  json += files + "]}";

  sendJson(200, json);
}

void searchRecursive(
  const String &path,
  const String &queryLower,
  String &result,
  int &count,
  int depth
) {
  if (depth > 12 || count >= 100) return;

  File dir = store().open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  File item = dir.openNextFile();

  while (item && count < 100) {
    String fullPath = String(item.path());
    String name = fileNameOnly(fullPath);
    bool isDir = item.isDirectory();
    uint64_t size = isDir ? 0 : item.size();

    item.close();

    if (!hiddenName(name)) {
      String lower = name;
      lower.toLowerCase();

      if (lower.indexOf(queryLower) >= 0) {
        if (count++) result += ",";
        result += itemJson(fullPath, name, isDir, size);
      }

      if (isDir) {
        searchRecursive(fullPath, queryLower, result, count, depth + 1);
      }
    }

    item = dir.openNextFile();
  }

  dir.close();
}

void handleSearch() {
  if (!requireAPI()) return;
  if (!server.hasArg("q")) {
    sendText(400, "Missing q");
    return;
  }

  String q = server.arg("q");
  q.trim();
  q.toLowerCase();

  if (q.length() == 0) {
    sendText(400, "Empty search");
    return;
  }

  String items = "";
  int count = 0;

  searchRecursive("/", q, items, count, 0);

  String json = "{";
  json += "\"node\":\"" + String(NODE_ID) + "\",";
  json += "\"count\":" + String(count) + ",";
  json += "\"items\":[" + items + "]}";

  sendJson(200, json);
}

// =====================================================
// Mutating operations
// =====================================================

void handleMkdir() {
  if (!requireAPI()) return;
  if (!server.hasArg("name")) {
    sendText(400, "Missing name");
    return;
  }

  String parent = server.hasArg("path") ? normalizePath(server.arg("path")) : "/";
  String name = server.arg("name");
  name.trim();

  if (!validSimpleName(name)) {
    sendText(400, "Invalid folder name");
    return;
  }

  String target = joinPath(parent, name);

  if (store().exists(target)) {
    sendText(409, "Already exists");
    return;
  }

  if (!store().mkdir(target)) {
    sendText(500, "mkdir failed");
    return;
  }

  sendJson(200, "{\"ok\":true}");
}

void handleDelete() {
  if (!requireAPI()) return;
  if (!server.hasArg("path")) {
    sendText(400, "Missing path");
    return;
  }

  String path = normalizePath(server.arg("path"));

  if (path == "/") {
    sendText(403, "Cannot delete root");
    return;
  }

  File item = store().open(path);

  if (!item) {
    sendText(404, "Not found");
    return;
  }

  bool isDir = item.isDirectory();
  item.close();

  bool ok = isDir ? store().rmdir(path) : store().remove(path);

  if (!ok) {
    sendText(500, isDir ? "Delete failed. Folder may not be empty." : "Delete failed");
    return;
  }

  sendJson(200, "{\"ok\":true}");
}

void handleRename() {
  if (!requireAPI()) return;
  if (!server.hasArg("path") || !server.hasArg("name")) {
    sendText(400, "Missing parameter");
    return;
  }

  String oldPath = normalizePath(server.arg("path"));
  String newName = server.arg("name");
  newName.trim();

  if (oldPath == "/" || !validSimpleName(newName)) {
    sendText(400, "Invalid rename");
    return;
  }

  String newPath = joinPath(parentPath(oldPath), newName);

  if (store().exists(newPath)) {
    sendText(409, "Destination already exists");
    return;
  }

  if (!store().rename(oldPath, newPath)) {
    sendText(500, "Rename failed");
    return;
  }

  sendJson(200, "{\"ok\":true}");
}

void handleMove() {
  if (!requireAPI()) return;
  if (!server.hasArg("path") || !server.hasArg("dest")) {
    sendText(400, "Missing parameter");
    return;
  }

  String oldPath = normalizePath(server.arg("path"));
  String dest = normalizePath(server.arg("dest"));

  if (oldPath == "/") {
    sendText(403, "Cannot move root");
    return;
  }

  File destFolder = store().open(dest);
  if (!destFolder || !destFolder.isDirectory()) {
    if (destFolder) destFolder.close();
    sendText(400, "Destination folder does not exist");
    return;
  }
  destFolder.close();

  File source = store().open(oldPath);
  if (!source) {
    sendText(404, "Source not found");
    return;
  }

  bool sourceIsDir = source.isDirectory();
  source.close();

  if (sourceIsDir && (dest == oldPath || dest.startsWith(oldPath + "/"))) {
    sendText(400, "Cannot move a folder into itself");
    return;
  }

  String newPath = joinPath(dest, fileNameOnly(oldPath));

  if (newPath == oldPath) {
    sendJson(200, "{\"ok\":true}");
    return;
  }

  if (store().exists(newPath)) {
    sendText(409, "Destination already contains this name");
    return;
  }

  if (!store().rename(oldPath, newPath)) {
    sendText(500, "Move failed");
    return;
  }

  sendJson(200, "{\"ok\":true}");
}

// =====================================================
// Download
// =====================================================

void handleDownload() {
  if (!requireAPI()) return;
  if (!server.hasArg("path")) {
    sendText(400, "Missing path");
    return;
  }

  String path = normalizePath(server.arg("path"));

  File file = store().open(path, FILE_READ);

  if (!file || file.isDirectory()) {
    if (file) file.close();
    sendText(404, "File not found");
    return;
  }

  String name = fileNameOnly(path);
  size_t fileSize = file.size();
  beginTransfer();

  cors();
  server.sendHeader(
    "Content-Disposition",
    "attachment; filename=\"" + name + "\""
  );
  server.setContentLength(fileSize);
  server.send(200, contentType(name), "");

  WiFiClient client = server.client();

  bool stalled = false;
  unsigned long lastWriteAt = millis();
  while (!stalled && file.available() && client.connected()) {
    size_t n = file.read(transferBuffer, sizeof(transferBuffer));

    if (n == 0) break;

    size_t sent = 0;
    while (sent < n && client.connected()) {
      size_t written = client.write(transferBuffer + sent, n - sent);
      if (written == 0) {
        // A peer can remain connected after it stops reading. Do not keep
        // the radio and CPU awake forever on an undrainable TCP socket.
        if (millis() - lastWriteAt >= 10000UL) {
          stalled = true;
          break;
        }
        serviceTransferDiscovery();
        delay(5);
        continue;
      }
      sent += written;
      lastWriteAt = millis();
    }

    yield();
    serviceTransferDiscovery();
  }

  file.close();
  if (stalled) client.stop();
  endTransfer();
}

// =====================================================
// Upload
// =====================================================

bool flushUploadBuffer() {
  const size_t pending = uploadBuffered;
  size_t writtenTotal = 0;

  while (writtenTotal < pending) {
    size_t written = uploadFile.write(
      transferBuffer + writtenTotal, pending - writtenTotal
    );
    if (written == 0) break;
    writtenTotal += written;
  }

  uploadBuffered = 0;
  return writtenTotal == pending;
}

void uploadHandler() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadAccepted = false;
    uploadComplete = false;
    uploadBuffered = 0;
    uploadFinalStatus = 200;
    uploadFinalMessage = "OK";

    if (!authorized()) {
      uploadFinalStatus = 401;
      uploadFinalMessage = "Unauthorized";
      return;
    }

    if (!storageReady) {
      uploadFinalStatus = 503;
      uploadFinalMessage = "Storage not ready";
      return;
    }

    String folder = server.hasArg("path") ? normalizePath(server.arg("path")) : "/";

    String filename = upload.filename;
    filename.replace("\\", "/");

    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);

    if (!validSimpleName(filename)) {
      uploadFinalStatus = 400;
      uploadFinalMessage = "Invalid filename";
      return;
    }

    String target = joinPath(folder, filename);

    if (store().exists(target)) store().remove(target);

    uploadFile = store().open(target, FILE_WRITE);

    if (!uploadFile) {
      uploadFinalStatus = 500;
      uploadFinalMessage = "Cannot create destination file";
      return;
    }

    uploadAccepted = true;
    beginTransfer();
  }

  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadAccepted && uploadFile) {
      const uint8_t *source = upload.buf;
      size_t remaining = upload.currentSize;

      while (remaining > 0) {
        size_t space = sizeof(transferBuffer) - uploadBuffered;
        size_t n = remaining < space ? remaining : space;
        memcpy(transferBuffer + uploadBuffered, source, n);
        uploadBuffered += n;
        source += n;
        remaining -= n;

        if (uploadBuffered == sizeof(transferBuffer) && !flushUploadBuffer()) {
          uploadFinalStatus = 500;
          uploadFinalMessage = "Storage write failed";
          uploadAccepted = false;
          break;
        }
      }
      serviceTransferDiscovery();
    }
  }

  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadAccepted && uploadFile &&
        uploadBuffered > 0 && !flushUploadBuffer()) {
      uploadFinalStatus = 500;
      uploadFinalMessage = "Storage write failed";
      uploadAccepted = false;
    }
    uploadComplete = uploadAccepted;
    uploadBuffered = 0;
    if (uploadFile) uploadFile.close();
    endTransfer();
  }

  else if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadBuffered = 0;
    if (uploadFile) uploadFile.close();
    uploadFinalStatus = 499;
    uploadFinalMessage = "Upload aborted";
    uploadAccepted = false;
    uploadComplete = false;
    endTransfer();
  }
}

void uploadFinished() {
  // WebServer can finish a request without an UPLOAD_FILE_END callback.
  if (!uploadComplete) {
    uploadBuffered = 0;
    if (uploadFile) uploadFile.close();
  }
  if (uploadFinalStatus == 200 && !uploadComplete) {
    uploadFinalStatus = 500;
    uploadFinalMessage = "Incomplete upload";
  }
  if (transferInProgress) endTransfer();
  cors();

  if (uploadFinalStatus == 200 && uploadComplete) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(uploadFinalStatus, "text/plain", uploadFinalMessage);
  }

  uploadAccepted = false;
  uploadComplete = false;
}

// Raw upload avoids WebServer's byte-by-byte multipart boundary scan. The
// request metadata is in headers because WebServer parses URL arguments only
// after all RAW_* callbacks have completed.
int rawHexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool decodeRawHeader(const String &encoded, String &decoded, size_t maxBytes) {
  if (encoded.length() > maxBytes * 3) return false;
  decoded = "";
  size_t reserveBytes = encoded.length() < maxBytes ? encoded.length() : maxBytes;
  if (!decoded.reserve(reserveBytes)) return false;

  for (size_t i = 0; i < encoded.length(); ++i) {
    uint8_t byte = (uint8_t)encoded[i];
    if (byte >= 0x80) return false;  // Headers carry ASCII percent encoding.
    if (byte == '%') {
      if (i + 2 >= encoded.length()) return false;
      int high = rawHexDigit(encoded[++i]);
      int low = rawHexDigit(encoded[++i]);
      if (high < 0 || low < 0) return false;
      byte = (uint8_t)((high << 4) | low);
    }
    if (byte < 0x20 || byte == 0x7f) return false;
    decoded += (char)byte;
    if (decoded.length() > maxBytes) return false;
  }
  return true;
}

bool parseRawLength(const String &text, size_t &length) {
  if (text.length() == 0 || text.length() > 10) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c < '0' || c > '9') return false;
    uint32_t digit = (uint32_t)(c - '0');
    if (value > (2147483647UL - digit) / 10UL) return false;
    value = value * 10UL + digit;
  }
  length = value;
  return true;
}

void rawUploadError(int status, const char *message) {
  uploadFinalStatus = status;
  uploadFinalMessage = message;
  uploadAccepted = false;
}

void rawUploadHandler() {
  HTTPRaw &raw = server.raw();

  if (raw.status == RAW_START) {
    if (uploadFile) uploadFile.close();
    rawUploadStarted = true;
    rawUploadExpected = 0;
    rawUploadReceived = 0;
    uploadAccepted = false;
    uploadComplete = false;
    uploadBuffered = 0;
    uploadFinalStatus = 200;
    uploadFinalMessage = "OK";

    if (server.header("X-Swiftwing-Key") != NODE_API_KEY) {
      rawUploadError(401, "Unauthorized");
      return;
    }
    if (!storageReady) {
      rawUploadError(503, "Storage not ready");
      return;
    }
    if (!server.header("Content-Type").equalsIgnoreCase("application/octet-stream") ||
        !parseRawLength(server.header("Content-Length"), rawUploadExpected)) {
      rawUploadError(400, "Invalid raw upload headers");
      return;
    }

    String encodedPath = server.header("X-Swiftwing-Path");
    String encodedName = server.header("X-Swiftwing-Name");
    String folder, filename;
    if (encodedPath.length() == 0 || encodedName.length() == 0 ||
        !decodeRawHeader(encodedPath, folder, 2048) ||
        !decodeRawHeader(encodedName, filename, 1024) ||
        !validSimpleName(filename)) {
      rawUploadError(400, "Invalid path or filename");
      return;
    }

    String target = joinPath(normalizePath(folder), filename);
    // Match the existing multipart overwrite and low-free-space behavior.
    if (store().exists(target)) store().remove(target);
    uploadFile = store().open(target, FILE_WRITE);
    if (!uploadFile) {
      rawUploadError(500, "Cannot create destination file");
      return;
    }
    uploadAccepted = true;
    beginTransfer();
  }

  else if (raw.status == RAW_WRITE) {
    if (!uploadAccepted || !uploadFile) return;
    const uint8_t *source = raw.buf;
    size_t remaining = raw.currentSize;
    while (remaining > 0) {
      size_t space = sizeof(transferBuffer) - uploadBuffered;
      size_t n = remaining < space ? remaining : space;
      memcpy(transferBuffer + uploadBuffered, source, n);
      uploadBuffered += n;
      rawUploadReceived += n;
      source += n;
      remaining -= n;
      if (uploadBuffered == sizeof(transferBuffer) && !flushUploadBuffer()) {
        rawUploadError(500, "Storage write failed");
        break;
      }
    }
    serviceTransferDiscovery();
  }

  else if (raw.status == RAW_END) {
    if (uploadAccepted && uploadFile && uploadBuffered > 0 &&
        !flushUploadBuffer()) {
      rawUploadError(500, "Storage write failed");
    }
    if (uploadAccepted &&
        (raw.totalSize != rawUploadExpected ||
         rawUploadReceived != rawUploadExpected)) {
      rawUploadError(500, "Incomplete raw upload");
    }
    if (uploadAccepted && uploadFile) {
      uploadFile.flush();
      if (uploadFile.size() != rawUploadExpected) {
        rawUploadError(500, "Storage write failed");
      }
    }
    uploadComplete = uploadAccepted;
    uploadBuffered = 0;
    if (uploadFile) uploadFile.close();
    if (transferInProgress) endTransfer();
  }

  else if (raw.status == RAW_ABORTED) {
    uploadBuffered = 0;
    if (uploadFile) uploadFile.close();
    rawUploadError(499, "Raw upload aborted");
    uploadComplete = false;
    rawUploadStarted = false;
    if (transferInProgress) endTransfer();
  }
}

void rawUploadFinished() {
  if (!rawUploadStarted) {
    rawUploadError(400, "Invalid raw upload");
    uploadComplete = false;
  }
  uploadFinished();
  rawUploadStarted = false;
}

// =====================================================
// Info + diagnostic page
// =====================================================

void handleInfo() {
  String json = "{";
  const uint64_t total = storageTotal();
  const uint64_t used = storageUsed();
  const uint64_t freeBytes = total >= used ? total - used : 0;
  json += "\"id\":\"" + String(NODE_ID) + "\",";
  json += "\"label\":\"" + String(NODE_LABEL) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"ready\":" + String(storageReady ? "true" : "false") + ",";
  json += "\"total\":" + String((unsigned long long)total) + ",";
  json += "\"used\":" + String((unsigned long long)used) + ",";
  json += "\"free\":" + String((unsigned long long)freeBytes) + ",";
  json += "\"rawUploadV1\":true,";
  json += "\"uptime\":" + String(millis() / 1000UL);
  json += "}";

  sendJson(200, json);
}

void handleRoot() {
  String text =
    String("Swiftwing ") + NODE_LABEL +
    "\nIP: " + WiFi.localIP().toString() +
    "\nStorage: " + (storageReady ? "READY" : "NOT READY") +
    "\nUse the Master UI for file management.\n";

  sendText(200, text);
}

void handleOptions() {
  cors();
  server.send(204, "text/plain", "");
}

// =====================================================
// Setup / loop
// =====================================================

void connectWiFi() {
  connectClusterWiFi();
}

void setupNode() {
  Serial.begin(115200);
  delay(500);

  connectWiFi();

  storageReady = beginStorage();

  Serial.print("Storage: ");
  Serial.println(storageReady ? "READY" : "NOT READY");

  discoveryUDP.begin(STORAGE_DISCOVERY_PORT);
  wifiSyncUDP.begin(WIFI_SYNC_PORT);

  const char *rawHeaders[] = {
    "Content-Type", "Content-Length", "X-Swiftwing-Key",
    "X-Swiftwing-Path", "X-Swiftwing-Name"
  };
  server.collectHeaders(rawHeaders, 5);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/list", HTTP_GET, handleList);
  server.on("/api/search", HTTP_GET, handleSearch);

  server.on("/api/mkdir", HTTP_POST, handleMkdir);
  server.on("/api/delete", HTTP_POST, handleDelete);
  server.on("/api/rename", HTTP_POST, handleRename);
  server.on("/api/move", HTTP_POST, handleMove);

  server.on("/download", HTTP_GET, handleDownload);

  server.on(
    "/upload",
    HTTP_POST,
    uploadFinished,
    uploadHandler
  );
  server.on("/upload-raw", HTTP_POST, rawUploadFinished, rawUploadHandler);

  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      handleOptions();
      return;
    }
    sendText(404, "Not found");
  });

  server.begin();

  Serial.print(NODE_LABEL);
  Serial.println(" ONLINE");
}

void loopNode() {
  server.handleClient();
  processDiscovery();
  processWiFiSync();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    connectWiFi();
    transferRadioAwake = false;
    transferInProgress = false;
  }

  updateTransferPower();
  delay(10);
}


void setup() {
  setupNode();
}

void loop() {
  loopNode();
}
