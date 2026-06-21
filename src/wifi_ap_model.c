#include "wifi_ap_model.h"

#include <string.h>

void wifi_ap_list_init( WifiApList* list )
{
  list->count = 0;
  list->selected = -1;
}

uint8_t wifi_ap_add( WifiApList* list, const WifiAp* ap )
{
  if ( list->count >= MAX_AP_RESULTS ) return 0;
  memcpy( &list->items[list->count], ap, sizeof( WifiAp ) );
  ++list->count;
  return 1;
}

const WifiAp* wifi_ap_selected( const WifiApList* list )
{
  if ( list->selected < 0 || list->selected >= list->count ) return 0;
  return &list->items[list->selected];
}
