#include "menu_ui.h"
#include "print.h"

#include <string.h>

#define FRAMEBUFFER ((void *)0x1000)

static void print_int( int x, int y, int value )
{
  char digits[7];
  uint8_t count = 0;
  unsigned int magnitude;
  if ( value < 0 ) { printChar( x, y, '-' ); x += 6; magnitude = (unsigned int)( -value ); }
  else magnitude = (unsigned int)value;
  if ( magnitude == 0 ) { printChar( x, y, '0' ); return; }
  while ( magnitude && count < sizeof( digits ) ) {
    digits[count++] = (char)( '0' + magnitude % 10 ); magnitude /= 10;
  }
  while ( count ) { printChar( x, y, digits[--count] ); x += 6; }
}

static void print_clipped( int x, int y, const char* text, uint8_t chars )
{
  while ( *text && chars-- ) { printChar( x, y, (unsigned char)*text++ ); x += 6; }
}

static void print_ssid( int x, int y, const char* ssid, uint8_t chars )
{
  /* Empty SSIDs are legitimate hidden networks, not parser failures. */
  print_clipped( x, y, *ssid ? ssid : "HIDDEN", chars );
}

static void draw_menu( const AppContext* app )
{
  uint8_t i;
  const char* labels[3] = { "SCAN", "SELECT", "ATTACK" };
  print( 6, 0, "PM WIFI LAB" );
  for ( i = 0; i < 3; ++i ) {
    printChar( 0, i + 2, app->menu_index == i ? '>' : ' ' );
    print( 8, i + 2, labels[i] );
  }
  if ( app->aps.selected >= 0 ) print( 6, 7, "TARGET SET" );
}

static void draw_results( const AppContext* app )
{
  uint8_t row;
  uint8_t index;
  print( 6, 0, "WIFI RESULTS" );
  for ( row = 0; row < 5; ++row ) {
    index = app->list_top + row;
    if ( index >= app->aps.count ) break;
    printChar( 0, row + 2, index == app->list_index ? '>' : ' ' );
    print_ssid( 7, row + 2, app->aps.items[index].ssid, 9 );
    print_int( 67, row + 2, app->aps.items[index].rssi );
  }
  print( 0, 7, "A:SEL B:BACK" );
}

static void draw_selected( const AppContext* app )
{
  const WifiAp* ap = wifi_ap_selected( &app->aps );
  print( 0, 0, "SELECTED:" );
  if ( !ap ) return;
  print_ssid( 0, 2, ap->ssid, 16 );
  print( 0, 3, "CH:" ); print_int( 24, 3, ap->channel );
  print( 48, 3, "RSSI:" ); print_int( 78, 3, ap->rssi );
  print_clipped( 0, 5, ap->bssid, 16 );
  if ( ap->bssid[16] ) print_clipped( 0, 6, ap->bssid + 16, 2 );
  print( 0, 7, "B:BACK" );
}

static void draw_deauth_monitor( const AppContext* app )
{
  const WifiAp* ap = wifi_ap_selected( &app->aps );
  print( 12, 0, "DEAUTH MON" );
  print( 0, 2, "TARGET:" );
  if ( ap ) print_ssid( 0, 3, ap->ssid, 16 );
  if ( app->state == APP_SIM_ATTACK_RUNNING ) {
    print( 0, 5, "DEAUTH:" ); print_int( 48, 5, app->sim_attack_deauth );
    print( 0, 6, "DISASSOC:" ); print_int( 60, 6, app->sim_attack_disassoc );
  } else print( 0, 5, "READY/WAIT" );
  print( 0, 7, "B:STOP/BACK" );
}

void menu_ui_draw( const AppContext* app )
{
  memset( FRAMEBUFFER, 0, 96 * 8 );
  if ( app->state == APP_IDLE ) draw_menu( app );
  else if ( app->state == APP_SCANNING ) {
    print( 12, 2, "SCANNING..." ); print( 12, 4, "PLEASE WAIT" );
  } else if ( app->state == APP_SCAN_RESULTS ) draw_results( app );
  else if ( app->state == APP_AP_SELECTED ) draw_selected( app );
  else if ( app->state == APP_SIM_ATTACK_READY || app->state == APP_SIM_ATTACK_RUNNING )
    draw_sim_attack_monitor( app );
  else {
    print( 24, 1, "ERROR" );
    print_clipped( 0, 3, app->error, 16 );
    if ( app->error[16] ) print_clipped( 0, 4, app->error + 16, 16 );
    print( 0, 7, "B:BACK" );
  }
}
