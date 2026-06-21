#include "app_state.h"

#include <string.h>

static void set_error( AppContext* app, const char* message )
{
  unsigned int i = 0;
  while ( message[i] && i + 1 < sizeof( app->error ) ) {
    app->error[i] = message[i]; ++i;
  }
  app->error[i] = '\0';
  app->state = APP_ERROR;
  app->dirty = 1;
}

static uint8_t append_uint( char* dst, uint8_t at, unsigned int value )
{
  char reverse[6];
  uint8_t count = 0;
  if ( value == 0 ) { dst[at++] = '0'; return at; }
  while ( value && count < sizeof( reverse ) ) {
    reverse[count++] = (char)( '0' + value % 10 ); value /= 10;
  }
  while ( count ) dst[at++] = reverse[--count];
  return at;
}

static void send_select( AppContext* app )
{
  char command[16] = "SELECT ";
  uint8_t at = 7;
  at = append_uint( command, at, app->aps.items[app->list_index].remote_index );
  command[at] = '\0';
  /* Selection is committed locally; remote acknowledgement is asynchronous. */
  serial_protocol_send( command );
}

static void send_simulation( AppContext* app )
{
  const WifiAp* ap = wifi_ap_selected( &app->aps );
  char command[48] = "SIM_DEAUTH ";
  uint8_t at = 11;
  unsigned int i = 0;
  if ( !ap ) { set_error( app, "SELECT AP FIRST" ); return; }
  while ( ap->bssid[i] && at + 1 < sizeof( command ) ) command[at++] = ap->bssid[i++];
  command[at++] = ' ';
  at = append_uint( command, at, ap->channel );
  command[at] = '\0';
  if ( !serial_protocol_send( command ) ) { set_error( app, "LINK BUSY" ); return; }
  app->state = APP_SIM_ATTACK_READY;
  app->dirty = 1;
}

void app_init( AppContext* app )
{
  app->state = APP_IDLE;
  wifi_ap_list_init( &app->aps );
  app->menu_index = 0;
  app->list_index = 0;
  app->list_top = 0;
  app->select_pending = 0;
  app->dirty = 1;
  app->error[0] = '\0';
  serial_protocol_init();
}

void app_handle_input( AppContext* app, uint8_t up, uint8_t down,
                       uint8_t accept, uint8_t back )
{
  if ( app->state == APP_IDLE ) {
    if ( up && app->menu_index ) { --app->menu_index; app->dirty = 1; }
    if ( down && app->menu_index < 2 ) { ++app->menu_index; app->dirty = 1; }
    if ( !accept ) return;
    if ( app->menu_index == 0 ) {
      if ( serial_protocol_busy() ) { set_error( app, "LINK BUSY" ); return; }
      wifi_ap_list_init( &app->aps );
      if ( !serial_protocol_send( "SCAN" ) ) { set_error( app, "CANNOT SEND" ); return; }
      app->state = APP_SCANNING;
      app->dirty = 1;
    } else if ( app->menu_index == 1 ) {
      if ( !app->aps.count ) { set_error( app, "NO SCAN RESULTS" ); return; }
      app->state = APP_SCAN_RESULTS;
      app->list_index = 0; app->list_top = 0; app->dirty = 1;
    } else send_simulation( app );
    return;
  }

  if ( app->state == APP_SCAN_RESULTS ) {
    if ( up && app->list_index ) --app->list_index;
    if ( down && app->list_index + 1 < app->aps.count ) ++app->list_index;
    if ( app->list_index < app->list_top ) app->list_top = app->list_index;
    if ( app->list_index >= app->list_top + 5 ) app->list_top = app->list_index - 4;
    if ( up || down ) app->dirty = 1;
    if ( accept ) {
      app->aps.selected = app->list_index;
      /* SCAN_DONE can precede the bridge's final DONE handshake. Queue the
       * SELECT so a fast button press is never lost. */
      app->select_pending = 1;
      app->state = APP_AP_SELECTED; app->dirty = 1;
    } else if ( back ) { app->state = APP_IDLE; app->dirty = 1; }
    return;
  }

  if ( app->state == APP_SIM_ATTACK_RUNNING && back ) {
    if ( !serial_protocol_busy() ) serial_protocol_send( "STOP" );
    return;
  }
  if ( back && app->state != APP_SCANNING ) {
    app->state = APP_IDLE; app->dirty = 1;
  }
}

static void handle_event( AppContext* app, SerialEvent* event )
{
  switch ( event->type ) {
    case SERIAL_EVENT_AP:
      if ( app->state == APP_SCANNING ) {
        wifi_ap_add( &app->aps, &event->ap );
        app->dirty = 1;
      }
      break;
    case SERIAL_EVENT_SCAN_DONE:
      if ( app->state == APP_SCANNING ) {
        if ( app->aps.count ) app->state = APP_SCAN_RESULTS;
        else set_error( app, "NO NETWORKS FOUND" );
        app->dirty = 1;
      }
      break;
    case SERIAL_EVENT_STATUS:
      if ( event->status == SIM_STATUS_RUNNING ) app->state = APP_SIM_ATTACK_RUNNING;
      else if ( event->status == SIM_STATUS_READY ) app->state = APP_SIM_ATTACK_READY;
      else if ( event->status == SIM_STATUS_STOPPED ) app->state = APP_AP_SELECTED;
      else set_error( app, "SIMULATION ERROR" );
      app->dirty = 1;
      break;
    case SERIAL_EVENT_ERROR: set_error( app, event->message ); break;
    case SERIAL_EVENT_INVALID: set_error( app, "INVALID RESPONSE" ); break;
    case SERIAL_EVENT_TIMEOUT: set_error( app, "ESP32 TIMEOUT" ); break;
    default: break;
  }
}

void app_poll( AppContext* app )
{
  SerialEvent event;
  if ( serial_protocol_poll( &event ) ) handle_event( app, &event );
  if ( app->select_pending && !serial_protocol_busy() ) {
    send_select( app );
    app->select_pending = 0;
  }
}

void app_tick( AppContext* app )
{
  SerialEvent event;
  if ( serial_protocol_tick( &event ) ) handle_event( app, &event );
}
