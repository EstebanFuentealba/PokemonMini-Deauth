#include "serial_protocol.h"

#include <string.h>

/*
 * RP2040 cartridge mailbox. The RP2040 owns UART; the ROM only exchanges
 * newline-delimited bytes here. DATA/ACK permits scans larger than 250 bytes.
 */
#define MB_BASE       ((volatile uint8_t _far *)0x1FFF00)
#define MB_MAGIC0     (MB_BASE[0])
#define MB_MAGIC1     (MB_BASE[1])
#define MB_OP         (MB_BASE[2])
#define MB_STATUS     (MB_BASE[3])
#define MB_LENGTH     (MB_BASE[4])
#define MB_SEQUENCE   (MB_BASE[5])
#define MB_PAYLOAD    ((volatile uint8_t _far *)&MB_BASE[6])
#define MB_PAYLOAD_SIZE 250

#define MB_OP_TEXT 1
#define MB_IDLE 0
#define MB_REQUEST 1
#define MB_DATA 2
#define MB_ACK 3
#define MB_DONE 4
#define MB_ERROR 5

static char line_buffer[SERIAL_MAX_LINE];
static uint8_t line_length;
static uint8_t chunk_offset;
static uint8_t chunk_length;
static uint8_t active;
static uint8_t overflow;
static unsigned int timeout_ticks;

static unsigned int parse_unsigned( const char* s, uint8_t* valid )
{
  unsigned int value = 0;
  *valid = 0;
  if ( !*s ) return 0;
  while ( *s ) {
    if ( *s < '0' || *s > '9' ) return 0;
    value = (unsigned int)( value * 10 + (unsigned int)( *s - '0' ) );
    *valid = 1;
    ++s;
  }
  return value;
}

static int parse_signed( const char* s, uint8_t* valid )
{
  int sign = 1;
  unsigned int value;
  if ( *s == '-' ) { sign = -1; ++s; }
  value = parse_unsigned( s, valid );
  return (int)value * sign;
}

static char* split_field( char** cursor )
{
  char* start = *cursor;
  char* p = start;
  while ( *p && *p != '|' ) ++p;
  if ( *p == '|' ) { *p = '\0'; *cursor = p + 1; }
  else *cursor = p;
  return start;
}

static void copy_text( char* dst, const char* src, unsigned int capacity )
{
  unsigned int i = 0;
  if ( !capacity ) return;
  while ( src[i] && i + 1 < capacity ) { dst[i] = src[i]; ++i; }
  dst[i] = '\0';
}

static void parse_line( SerialEvent* event )
{
  uint8_t valid;
  char* cursor;
  char* field;
  unsigned int value;

  event->type = SERIAL_EVENT_INVALID;
  event->message[0] = '\0';

  if ( !strcmp( line_buffer, "OK" ) ) event->type = SERIAL_EVENT_OK;
  else if ( !strcmp( line_buffer, "PONG" ) ) event->type = SERIAL_EVENT_PONG;
  else if ( !strncmp( line_buffer, "ERR ", 4 ) ) {
    event->type = SERIAL_EVENT_ERROR;
    copy_text( event->message, line_buffer + 4, sizeof( event->message ) );
  } else if ( !strncmp( line_buffer, "SCAN_DONE ", 10 ) ) {
    value = parse_unsigned( line_buffer + 10, &valid );
    if ( valid ) { event->type = SERIAL_EVENT_SCAN_DONE; event->count = value; }
  } else if ( !strncmp( line_buffer, "STATUS ", 7 ) ) {
    char* state = line_buffer + 7;
    event->type = SERIAL_EVENT_STATUS;
    event->status = SIM_STATUS_ERROR;
    if ( !strcmp( state, "READY" ) ) event->status = SIM_STATUS_READY;
    else if ( !strcmp( state, "RUNNING" ) ) event->status = SIM_STATUS_RUNNING;
    else if ( !strcmp( state, "STOPPED" ) ) event->status = SIM_STATUS_STOPPED;
    else if ( strcmp( state, "ERROR" ) ) event->type = SERIAL_EVENT_INVALID;
  } else if ( !strncmp( line_buffer, "MONITOR ", 8 ) ) {
    cursor = line_buffer + 8;
    field = split_field( &cursor );
    event->count = parse_unsigned( field, &valid );
    if ( !valid || *cursor == '\0' ) return;
    event->count2 = parse_unsigned( cursor, &valid );
    if ( valid ) event->type = SERIAL_EVENT_MONITOR;
  } else if ( !strncmp( line_buffer, "AP ", 3 ) ) {
    cursor = line_buffer + 3;
    field = split_field( &cursor );
    value = parse_unsigned( field, &valid );
    if ( !valid || value > 255 ) return;
    event->ap.remote_index = (uint8_t)value;
    field = split_field( &cursor );
    copy_text( event->ap.ssid, field, sizeof( event->ap.ssid ) );
    field = split_field( &cursor );
    if ( strlen( field ) != BSSID_LEN - 1 ) return;
    copy_text( event->ap.bssid, field, sizeof( event->ap.bssid ) );
    field = split_field( &cursor );
    value = parse_unsigned( field, &valid );
    if ( !valid || value > 255 ) return;
    event->ap.channel = (uint8_t)value;
    field = split_field( &cursor );
    event->ap.rssi = parse_signed( field, &valid );
    if ( valid ) event->type = SERIAL_EVENT_AP;
  }
}

void serial_protocol_init( void )
{
  line_length = 0;
  chunk_offset = 0;
  chunk_length = 0;
  active = 0;
  overflow = 0;
  timeout_ticks = 0;
  MB_MAGIC0 = 'P';
  MB_MAGIC1 = 'M';
  MB_STATUS = MB_IDLE;
}

uint8_t serial_protocol_send( const char* command )
{
  unsigned int length = strlen( command );
  unsigned int i;
  if ( active || length + 1 > MB_PAYLOAD_SIZE ) return 0;
  for ( i = 0; i < length; ++i ) MB_PAYLOAD[i] = (uint8_t)command[i];
  MB_PAYLOAD[length++] = '\n';
  MB_LENGTH = (uint8_t)length;
  ++MB_SEQUENCE;
  MB_OP = MB_OP_TEXT;
  MB_STATUS = MB_REQUEST;
  active = 1;
  line_length = 0;
  chunk_length = 0;
  overflow = 0;
  timeout_ticks = 0;
  return 1;
}

uint8_t serial_protocol_poll( SerialEvent* event )
{
  char c;
  event->type = SERIAL_EVENT_NONE;
  if ( !active ) return 0;

  if ( MB_STATUS == MB_ERROR ) {
    active = 0;
    event->type = SERIAL_EVENT_ERROR;
    copy_text( event->message, "RP2040 LINK", sizeof( event->message ) );
    MB_STATUS = MB_IDLE;
    return 1;
  }
  if ( MB_STATUS == MB_DONE ) {
    active = 0;
    MB_STATUS = MB_IDLE;
    if ( line_length || overflow ) {
      line_length = 0;
      overflow = 0;
      event->type = SERIAL_EVENT_INVALID;
      return 1;
    }
    return 0;
  }
  if ( MB_STATUS != MB_DATA ) return 0;

  if ( chunk_length == 0 ) {
    chunk_length = MB_LENGTH;
    if ( chunk_length > MB_PAYLOAD_SIZE ) chunk_length = MB_PAYLOAD_SIZE;
    chunk_offset = 0;
    timeout_ticks = 0;
  }

  while ( chunk_offset < chunk_length ) {
    c = (char)MB_PAYLOAD[chunk_offset++];
    if ( c == '\r' ) continue;
    if ( c == '\n' ) {
      line_buffer[line_length] = '\0';
      if ( chunk_offset >= chunk_length ) { chunk_length = 0; MB_STATUS = MB_ACK; }
      if ( overflow || line_length == 0 ) {
        overflow = 0; line_length = 0;
        event->type = SERIAL_EVENT_INVALID;
      } else {
        parse_line( event );
        line_length = 0;
      }
      return 1;
    }
    if ( line_length + 1 < SERIAL_MAX_LINE ) line_buffer[line_length++] = c;
    else overflow = 1;
  }
  chunk_length = 0;
  MB_STATUS = MB_ACK;
  return 0;
}

uint8_t serial_protocol_busy( void ) { return active; }

uint8_t serial_protocol_tick( SerialEvent* event )
{
  event->type = SERIAL_EVENT_NONE;
  if ( !active ) return 0;
  if ( ++timeout_ticks < SERIAL_TIMEOUT_TICKS ) return 0;
  active = 0;
  MB_STATUS = MB_IDLE;
  event->type = SERIAL_EVENT_TIMEOUT;
  return 1;
}
