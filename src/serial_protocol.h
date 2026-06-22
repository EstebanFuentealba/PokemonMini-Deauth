#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include "wifi_ap_model.h"
#include <stdint.h>

/* WiFi scans may take several seconds. At roughly 60 PRC ticks/s this gives
 * the ESP32 up to 15 seconds while still keeping the UI non-blocking. */
#define SERIAL_TIMEOUT_TICKS 900
#define SERIAL_MAX_LINE 96

typedef enum {
  SERIAL_EVENT_NONE,
  SERIAL_EVENT_OK,
  SERIAL_EVENT_ERROR,
  SERIAL_EVENT_AP,
  SERIAL_EVENT_SCAN_DONE,
  SERIAL_EVENT_STATUS,
  SERIAL_EVENT_MONITOR,
  SERIAL_EVENT_PONG,
  SERIAL_EVENT_INVALID,
  SERIAL_EVENT_TIMEOUT
} SerialEventType;

typedef enum {
  SIM_STATUS_NONE,
  SIM_STATUS_READY,
  SIM_STATUS_RUNNING,
  SIM_STATUS_STOPPED,
  SIM_STATUS_ERROR
} SimStatus;

typedef struct {
  SerialEventType type;
  WifiAp ap;
  unsigned int count;
  unsigned int count2;
  SimStatus status;
  char message[33];
} SerialEvent;

void serial_protocol_init( void );
uint8_t serial_protocol_send( const char* command );
uint8_t serial_protocol_poll( SerialEvent* event );
uint8_t serial_protocol_busy( void );
uint8_t serial_protocol_tick( SerialEvent* event );

#endif
