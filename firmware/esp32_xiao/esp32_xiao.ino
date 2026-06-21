#include <WiFi.h>

/* Safe lab endpoint: this sketch never enables promiscuous mode or transmits
 * management frames. SIM_DEAUTH only exercises the command/status path. */
static char inputLine[96];
static size_t inputLength;
static int scanCount;

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
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  scanCount = WiFi.scanNetworks(false, true);
  if (scanCount < 0) {
    Serial1.println("ERR 10 SCAN_FAILED");
    scanCount = 0;
    return;
  }
  for (i = 0; i < scanCount; ++i) {
    Serial1.print("AP "); Serial1.print(i); Serial1.write('|');
    printSafe(WiFi.SSID(i).c_str()); Serial1.write('|');
    Serial1.print(WiFi.BSSIDstr(i)); Serial1.write('|');
    Serial1.print(WiFi.channel(i)); Serial1.write('|');
    Serial1.println(WiFi.RSSI(i));
  }
  Serial1.print("SCAN_DONE "); Serial1.println(scanCount);
  WiFi.scanDelete();
}

static void handleCommand(char* line) {
  if (!strcmp(line, "SCAN")) {
    handleScan();
  } else if (!strcmp(line, "PING")) {
    Serial1.println("PONG");
  } else if (!strcmp(line, "STOP")) {
    Serial1.println("STATUS STOPPED");
  } else if (!strncmp(line, "SELECT ", 7)) {
    const char* indexText = line + 7;
    int index;
    if (!validUnsigned(indexText)) { Serial1.println("ERR 20 BAD_INDEX"); return; }
    index = atoi(indexText);
    if (index < 0 || index >= scanCount) Serial1.println("ERR 21 INDEX_RANGE");
    else Serial1.println("OK");
  } else if (!strncmp(line, "SIM_DEAUTH ", 11)) {
    /* Syntax is accepted only to simulate lifecycle states. No RF action. */
    char* separator = strrchr(line + 11, ' ');
    if (!separator || strlen(line + 11) < 19 || !validUnsigned(separator + 1)) {
      Serial1.println("STATUS ERROR");
      return;
    }
    Serial1.println("STATUS READY");
    Serial1.println("STATUS RUNNING");
  } else {
    Serial1.println("ERR 1 UNKNOWN_COMMAND");
  }
}

void setup() {
  /* XIAO ESP32-S3 D6=TX/GPIO43 and D7=RX/GPIO44 by default. Adjust to wiring. */
  Serial1.begin(115200, SERIAL_8N1, D7, D6);
  inputLength = 0;
  scanCount = 0;
}

void loop() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      inputLine[inputLength] = '\0';
      if (inputLength) handleCommand(inputLine);
      inputLength = 0;
    } else if (inputLength + 1 < sizeof(inputLine)) {
      inputLine[inputLength++] = c;
    } else {
      inputLength = 0;
      Serial1.println("ERR 2 LINE_TOO_LONG");
    }
  }
}
