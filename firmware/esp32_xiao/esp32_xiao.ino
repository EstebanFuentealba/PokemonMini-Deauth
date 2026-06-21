#include <WiFi.h>

/* Safe lab endpoint: this sketch never enables promiscuous mode or transmits
 * management frames. SIM_DEAUTH only exercises the command/status path. */
static char inputLine[96];
static size_t inputLength;
static int scanCount;
static uint32_t rxBytes;
static uint32_t commandCount;
static uint32_t lastHeartbeat;

static void logPrefix(const char* level) {
  Serial.print('[');
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(level);
  Serial.print("] ");
}

static void logLine(const char* level, const char* message) {
  logPrefix(level);
  Serial.println(message);
}

static void sendLine(const char* response) {
  Serial1.println(response);
  logPrefix("TX");
  Serial.println(response);
}

static void logRxByte(uint8_t value) {
  logPrefix("RX BYTE");
  Serial.print("0x");
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
  if (value >= 32 && value <= 126) {
    Serial.print(" '");
    Serial.write(value);
    Serial.print("'");
  }
  Serial.println();
}

static void printSafe(const char* text) {
  while (*text) {
    char c = *text++;
    Serial1.write((c == '|' || c == '\r' || c == '\n') ? '_' : c);
  }
}

static bool validUnsigned(const char* text) {
  if (!*text) return false;
  while (*text) {
    if (*text < '0' || *text > '9') return false;
    ++text;
  }
  return true;
}

static void handleScan() {
  int i;
  uint32_t startedAt = millis();

  /* Confirm receipt before the blocking radio scan starts. */
  sendLine("OK");
  Serial1.flush();
  logLine("SCAN", "Starting WiFi scan");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  scanCount = WiFi.scanNetworks(false, true);
  if (scanCount < 0) {
    sendLine("ERR 10 SCAN_FAILED");
    logLine("ERROR", "WiFi.scanNetworks failed");
    scanCount = 0;
    return;
  }
  logPrefix("SCAN");
  Serial.print("Found ");
  Serial.print(scanCount);
  Serial.print(" AP(s) in ");
  Serial.print(millis() - startedAt);
  Serial.println(" ms");
  for (i = 0; i < scanCount; ++i) {
    Serial1.print("AP "); Serial1.print(i); Serial1.write('|');
    printSafe(WiFi.SSID(i).c_str()); Serial1.write('|');
    Serial1.print(WiFi.BSSIDstr(i)); Serial1.write('|');
    Serial1.print(WiFi.channel(i)); Serial1.write('|');
    Serial1.println(WiFi.RSSI(i));

    logPrefix("TX AP");
    Serial.print(i); Serial.print(" SSID="); Serial.print(WiFi.SSID(i));
    Serial.print(" BSSID="); Serial.print(WiFi.BSSIDstr(i));
    Serial.print(" CH="); Serial.print(WiFi.channel(i));
    Serial.print(" RSSI="); Serial.println(WiFi.RSSI(i));
  }
  Serial1.print("SCAN_DONE "); Serial1.println(scanCount);
  logPrefix("TX"); Serial.print("SCAN_DONE "); Serial.println(scanCount);
  Serial1.flush();
  WiFi.scanDelete();
}

static void handleCommand(char* line) {
  ++commandCount;
  logPrefix("RX CMD");
  Serial.println(line);
  if (!strcmp(line, "SCAN")) {
    handleScan();
  } else if (!strcmp(line, "PING")) {
    sendLine("PONG");
  } else if (!strcmp(line, "STOP")) {
    sendLine("STATUS STOPPED");
  } else if (!strncmp(line, "SELECT ", 7)) {
    const char* indexText = line + 7;
    int index;
    if (!validUnsigned(indexText)) { sendLine("ERR 20 BAD_INDEX"); return; }
    index = atoi(indexText);
    if (index < 0 || index >= scanCount) sendLine("ERR 21 INDEX_RANGE");
    else sendLine("OK");
  } else if (!strncmp(line, "SIM_DEAUTH ", 11)) {
    /* Syntax is accepted only to simulate lifecycle states. No RF action. */
    char* separator = strrchr(line + 11, ' ');
    if (!separator || strlen(line + 11) < 19 || !validUnsigned(separator + 1)) {
      sendLine("STATUS ERROR");
      return;
    }
    sendLine("STATUS READY");
    sendLine("STATUS RUNNING");
  } else {
    sendLine("ERR 1 UNKNOWN_COMMAND");
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  /* XIAO ESP32-S3 D6=TX/GPIO43 and D7=RX/GPIO44 by default. Adjust to wiring. */
  Serial1.begin(115200, SERIAL_8N1, D7, D6);
  inputLength = 0;
  scanCount = 0;
  rxBytes = 0;
  commandCount = 0;
  lastHeartbeat = millis();

  logLine("BOOT", "PM WiFi Lab ESP32-S3 starting");
  logLine("UART", "Serial1 115200 8N1 RX=D7/GPIO44 TX=D6/GPIO43");
  logLine("READY", "Waiting for RP2040 commands");
}

void loop() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    ++rxBytes;
    logRxByte((uint8_t)c);
    if (c == '\r') continue;
    if (c == '\n') {
      inputLine[inputLength] = '\0';
      if (inputLength) handleCommand(inputLine);
      inputLength = 0;
    } else if (inputLength + 1 < sizeof(inputLine)) {
      inputLine[inputLength++] = c;
    } else {
      inputLength = 0;
      sendLine("ERR 2 LINE_TOO_LONG");
      logLine("ERROR", "UART input line exceeded 95 bytes");
    }
  }

  if (millis() - lastHeartbeat >= 5000) {
    lastHeartbeat = millis();
    logPrefix("ALIVE");
    Serial.print("uptime="); Serial.print(lastHeartbeat);
    Serial.print("ms rx_bytes="); Serial.print(rxBytes);
    Serial.print(" commands="); Serial.println(commandCount);
  }
}
