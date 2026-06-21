#ifndef WIFI_AP_MODEL_H
#define WIFI_AP_MODEL_H

#include <stdint.h>

#define MAX_AP_RESULTS 32
#define MAX_SSID_LEN 32
#define BSSID_LEN 18

typedef struct {
  uint8_t remote_index;
  char ssid[MAX_SSID_LEN + 1];
  char bssid[BSSID_LEN];
  uint8_t channel;
  int rssi;
} WifiAp;

typedef struct {
  WifiAp items[MAX_AP_RESULTS];
  uint8_t count;
  int selected;
} WifiApList;

void wifi_ap_list_init( WifiApList* list );
uint8_t wifi_ap_add( WifiApList* list, const WifiAp* ap );
const WifiAp* wifi_ap_selected( const WifiApList* list );

#endif
