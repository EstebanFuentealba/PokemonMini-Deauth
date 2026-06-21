#include "pm.h"
#include "app_state.h"
#include "menu_ui.h"

#include <stdint.h>

volatile uint8_t flag;

_interrupt( 2 ) void prc_frame_copy_irq( void )
{
  flag = 1;
  IRQ_ACT1 = IRQ1_PRC_COMPLETE;
}

static uint8_t key_pressed( uint8_t keys, uint8_t previous, uint8_t key )
{
  return (uint8_t)( !( keys & key ) && ( previous & key ) );
}

int main( void )
{
  AppContext app;
  uint8_t keys = KEY_PAD;
  uint8_t previous = keys;

  PRI_KEY( 0x03 );
  IRQ_ENA3 = IRQ3_KEYPOWER;
  PRI_PRC( 0x01 );
  IRQ_ENA1 = IRQ1_PRC_COMPLETE;

  app_init( &app );
  menu_ui_draw( &app );

  for ( ;; ) {
    previous = keys;
    keys = KEY_PAD;

    app_handle_input(
      &app,
      key_pressed( keys, previous, KEY_UP ),
      key_pressed( keys, previous, KEY_DOWN ),
      key_pressed( keys, previous, KEY_A ),
      key_pressed( keys, previous, KEY_B )
    );

    /* Consume at most one parsed line per pass, keeping input responsive. */
    app_poll( &app );

    /* PRC completion is our inexpensive, hardware-derived timeout clock. */
    if ( flag ) {
      flag = 0;
      app_tick( &app );
    }

    if ( app.dirty ) {
      menu_ui_draw( &app );
      app.dirty = 0;
    }
  }
}
