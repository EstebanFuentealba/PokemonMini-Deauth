#ifndef APP_STATE_H
#define APP_STATE_H

#include "serial_protocol.h"
#include "wifi_ap_model.h"
#include <stdint.h>

typedef enum {
  APP_IDLE,
  APP_SCANNING,
  APP_SCAN_RESULTS,
  APP_AP_SELECTED,
  APP_SIM_ATTACK_READY,
  APP_SIM_ATTACK_RUNNING,
  APP_ERROR
} AppState;

typedef struct {
  AppState state;
  WifiApList aps;
  uint8_t menu_index;
  uint8_t list_index;
  uint8_t list_top;
  uint8_t select_pending;
  uint8_t monitor_poll_ticks;
  unsigned int sim_attack_deauth;
  unsigned int sim_attack_disassoc;
  uint8_t dirty;
  char error[33];
} AppContext;

void app_init( AppContext* app );
void app_handle_input( AppContext* app, uint8_t up, uint8_t down,
                       uint8_t accept, uint8_t back );
void app_poll( AppContext* app );
void app_tick( AppContext* app );

#endif
