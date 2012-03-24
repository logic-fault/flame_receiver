
#include "io430.h"

/*#define GRAPHICS_ENABLED*/

#ifdef GRAPHICS_ENABLED
#include "lcd_graphics_arrays.h"
#endif /* GRAPHICS_ENABLED */

#include "graphics.h"
#include "radio.h"
#include "spi.h"

typedef struct
{
  unsigned char feed_enabled;
  unsigned char release_enabled;
  unsigned char charged;
} system_state_t;

lcd_state_t lcd_state = { 0, 1, 1 };


void handleRadioSignal(radio_signal_t sig)
{
  switch (sig)
  {
  case SIG_FIRE_START:
    // ensure fill tank has filled, close fill tank, open release for X seconds
    break;
  case SIG_FIRE_ABORT:
    // disable timer for release
    break;
  case SIG_ARM:
    // open fill tank, close after specified time
    lcd_state.armed_enabled = 1;
    break;
  case SIG_DISARM:
    // disable timer for release, disable filling of tank or releases
    lcd_state.armed_enabled = 0;
    break;
  case SIG_NONE:
    // intentional fall-through
  default:
    // invalid signal received
    break;
  }
}



unsigned char LCD_COLUMNS  = 128;
unsigned char LCD_SEGMENTS =   8;

 unsigned char lcdInit[] = {
                             0x40,
                             0xa1,
                             0xc0,
                             0xa6,
                             0xa2,
                             0x2f,
                             0xf8,
                             0x00,
                             0x27,
                             0x81,
                             0x16,
                             0xac,
                             0x00,
                             0xaf,
                         };

int main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  P1DIR  = 0x1f;
  P1OUT  = 0x01; // enable flag for SPI & reset inactive
  for (int i = 0; i < 100; i++);
  
  
  setupSpi();
  setupRadio();
  
  sendSpiStringLen(lcdInit, 14, DEV_LCD);
  sendSpiChar(0xb0, DEV_LCD); // page 0
  // write a block to ram
  
  // initialize the lcd
#ifdef GRAPHICS_ENABLED
  writeBoxFromGraphics(&full_box, &full_box);
#endif
  
  while (1)
  {
     
      handleRadioSignal(checkRadioSignal());

#ifdef GRAPHICS_ENABLED
      // if (lcd_state.armed_enabled)
      //{
         //writeBoxFromGraphics(feed_box);
      //}
    
      if (lcd_state.feed_enabled)
         writeBoxFromGraphics( &release_on_box, &feed_off_box );
      else
         writeBoxFromGraphics( &feed_off_box, &feed_off_box);
      
      
      if (!lcd_state.release_enabled)
         writeBoxFromGraphics( &feed_off_box, &release_on_box );
      else
         writeBoxFromGraphics( &release_on_box, &release_on_box);
#endif    
      
      // enable to test flipping every cycle
      //lcd_state.feed_enabled = !lcd_state.feed_enabled;
      //P1OUT = P1OUT ^ 0x06;
  }
   
  return 0;
}

