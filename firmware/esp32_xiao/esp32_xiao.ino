#include <WiFi.h>

/* Safe lab endpoint: this sketch never enables promiscuous mode or transmits
 * management frames. SIM_DEAUTH only exercises the command/status path. */
static char inputLine[96];
static size_t inputLength;
static char usbInputLine[96];
static size_t usbInputLength;
static int scanCount;
static const int maxReportedAps = 32;
static uint32_t rxBytes;
static uint32_t commandCount;
static uint32_t lastHeartbeat;
static uint32_t lastUartByteAt;

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
  if (rxBytes == 65) {
    logLine("RX BYTE", "Further raw-byte logs suppressed; counters remain active");
  }
  if (rxBytes > 64) return;
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
  int reportedCount;
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
  reportedCount = scanCount < maxReportedAps ? scanCount : maxReportedAps;
  if (scanCount > reportedCount) {
    logPrefix("SCAN");
    Serial.print("Limiting UART response to ");
    Serial.print(reportedCount);
    Serial.println(" AP(s)");
  }
  for (i = 0; i < reportedCount; ++i) {
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
  Serial1.print("SCAN_DONE "); Serial1.println(reportedCount);
  logPrefix("TX"); Serial.print("SCAN_DONE "); Serial.println(reportedCount);
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
  /* UART idle is HIGH. This prevents a disconnected RP2040 TX from floating. */
  pinMode(D7, INPUT_PULLUP);
  Serial1.begin(115200, SERIAL_8N1, D7, D6);
  inputLength = 0;
  usbInputLength = 0;
  scanCount = 0;
  rxBytes = 0;
  commandCount = 0;
  lastHeartbeat = millis();
  lastUartByteAt = 0;

  logLine("BOOT", "PM WiFi Lab ESP32-S3 starting");
  logLine("UART", "Serial1 115200 8N1 RX=D7/GPIO44 TX=D6/GPIO43");
  logLine("READY", "Waiting for RP2040 commands; USB console accepts PING or SCAN");
}

void loop() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    ++rxBytes;
    lastUartByteAt = millis();
    logRxByte((uint8_t)c);
    if (c == '\r') continue;
    if (c == '\n') {
      inputLine[inputLength] = '\0';
      if (inputLength) handleCommand(inputLine);
      inputLength = 0;
    } else if ((uint8_t)c < 32 || (uint8_t)c > 126) {
      /* The protocol is printable ASCII. Drop binary noise immediately so it
       * cannot prefix and corrupt the next real command. */
      inputLength = 0;
    } else if (inputLength + 1 < sizeof(inputLine)) {
      inputLine[inputLength++] = c;
    } else {
      inputLength = 0;
      sendLine("ERR 2 LINE_TOO_LONG");
      logLine("ERROR", "UART input line exceeded 95 bytes");
    }
  }

  /* A truncated/noisy line must not survive until a later valid command. */
  if (inputLength && millis() - lastUartByteAt > 250) {
    logLine("UART", "Discarding incomplete line after 250 ms idle");
    inputLength = 0;
  }

  /* USB diagnostic console: type PING or SCAN in Serial Monitor using a
   * newline ending to test the ESP32 without depending on the RP2040. */
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      usbInputLine[usbInputLength] = '\0';
      if (usbInputLength) {
        logPrefix("USB CMD"); Serial.println(usbInputLine);
        handleCommand(usbInputLine);
      }
      usbInputLength = 0;
    } else if (usbInputLength + 1 < sizeof(usbInputLine)) {
      usbInputLine[usbInputLength++] = c;
    } else {
      usbInputLength = 0;
      logLine("ERROR", "USB command exceeded 95 bytes");
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
