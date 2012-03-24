
#include "io430.h"

/*#define GRAPHICS_ENABLED*/

#ifdef GRAPHICS_ENABLED
#include "lcd_graphics_arrays.h"
#endif /* GRAPHICS_ENABLED */

typedef struct
{
   unsigned char x;
   unsigned char y;
} xy_pair_t;

typedef struct
{
   xy_pair_t top_left;
   xy_pair_t bottom_right;
} box_t;

typedef struct
{
   unsigned char feed_enabled;
   unsigned char release_enabled;
   unsigned char armed_enabled;
} lcd_state_t;

typedef struct
{
  unsigned char fifo[2];
  unsigned char status[2];
} radio_status_t;


const int RADIO_SYNC_WORD = 0x77a5;

// latest message sent
unsigned char radio_msg[4];

radio_status_t radio_status;

lcd_state_t lcd_state = { 0, 1, 1 };

const  box_t feed_off_box    = { { 108, 17 } , { 127, 28 } } ;
const box_t release_on_box = { { 108, 33 } , { 127, 44 } } ;
const box_t dis_box     = { {   1, 20 } , {  17, 28 } } ;
const box_t full_box = { { 0, 0, } , {127, 63} };

typedef enum { DEV_RADIO, DEV_LCD } spi_device_t;

typedef enum { SIG_NONE = 0,       // no signal received
               SIG_FIRE_START, // initiate charge or fire sequence
               SIG_FIRE_ABORT, // stop firing sequence, if in progress
               SIG_ARM,        // allow firing
               SIG_DISARM      // disallow firing
             } radio_signal_t;

unsigned char sendSpiChar(unsigned char c, spi_device_t dev);

void setupSpi()
{
   // setup SPI
   USICKCTL = 0x48; // main clock, divide by 128
   USICTL0  = 0xEA; //11101010 spi outputs enabled; not held in reset state
   USICTL1 |= 0x80; // data captured on leading clock edge
   //USICTL0 &= ~0x01; // enable spi
   P1OUT &= ~0x10;
}

void getRadioStatus(radio_status_t * stat)
{
  /* Check radio status for first two bytes, read output on last byte */
  P1OUT |= 0x10;
  stat->status[0] = sendSpiChar(0x00, DEV_RADIO);
  stat->status[1] = sendSpiChar(0x00, DEV_RADIO);
  
  // if a fifo interrupt is present (at least 8 bits data available), grab data
  if ( (stat->status[0] & 0x80) != 0)
     stat->fifo[0]   = sendSpiChar(0x00, DEV_RADIO);
  P1OUT &= 0x10;
  
  for (int i = 0; i < 10; i++);
}

radio_signal_t checkRadioSignal()
{
  static unsigned char radio_msg[4];
  static unsigned char msg_write_ptr = 0;
  getRadioStatus(&radio_status);
  if ( (radio_status.status[0] & 0x80) != 0)
  {
     /// data was valid
     int y = 1;
  }
  else
    return SIG_NONE;
}

void setupRadio()
{
  
  getRadioStatus(&radio_status);
  
  // 915 MHz band, 200khz bandwith, 8.5pF crystal, crystal always running no low bat or wakeup timer
  P1OUT |= 0x10;
  sendSpiChar(0x99, DEV_RADIO);
  sendSpiChar(0x08, DEV_RADIO);
  P1OUT &= 0x10;
  
  for (int i = 0; i < 10; i++);
  
  // frequency setting command, F=255
  P1OUT |= 0x10;
  sendSpiChar(0xa0, DEV_RADIO);
  sendSpiChar(0xff, DEV_RADIO);
  P1OUT &= 0x10;
  
  for (int i = 0; i < 10; i++);
  
  // bitrate = 600 bps
  P1OUT |= 0x10;
  sendSpiChar(0xc8, DEV_RADIO);
  sendSpiChar(0xc7, DEV_RADIO);
  P1OUT &= 0x10;
  
  for (int i = 0; i < 10; i++);
 
  // always fill fifo
  P1OUT |= 0x10;
  sendSpiChar(0xce, DEV_RADIO);
  sendSpiChar(0x87, DEV_RADIO); // 8f for always, 87 for sync word
  P1OUT &= 0x10;
  
  for (int i = 0; i < 10; i++);  
}

unsigned char sendSpiChar(unsigned char c, spi_device_t dev)
{
   USISRL = c;     // set the output shift register with our 8 bits
   if ( dev == DEV_LCD)
      P1OUT |= 0x10; // Slave_Select_BAR
   else
      P1OUT &= ~0x10;
   USICTL0 &= ~0x01; // break out of reset / disabled state
   int j = 1; // need this or spi will break
   USICNT = 0x08;  // we have 8 bits to send
   while ((USICNT & 0x1f) > 0); // make sure we sent all 8 bits
   //for (int i = 4; i > 0; i--);
   int i = 1; // need this or spi will break
   // disable spi
   USICTL0 |= 0x01;
   //P1OUT &= ~0x10;  // Slave_Select_BAR
   return USISRL;
}


void sendSpiStringLen(unsigned char * sz, int len, spi_device_t dev)
{
  int i = 0;
  for (; len--; len > 0) 
  {
    sendSpiChar(*(sz + i++), dev);
    for (int i = 0; i < 32000; i++); 
  }
}

unsigned char LCD_COLUMNS  = 128;
unsigned char LCD_SEGMENTS =   8;


// boxes must be same size, but we don't check due to code size limitations
void writeBoxFromGraphics(const box_t * source, const box_t * dest)
{
#ifdef GRAPHICS_ENABLED
  for (unsigned char i = dest->top_left.x; i <= dest->bottom_right.x; i++)
  {
     unsigned char page_start = dest->top_left.y / 8;
     unsigned char page_end   = dest->bottom_right.y / 8;
     unsigned char row_number_start_page;
     unsigned char row_number_stop_page;
     
     unsigned char dest_top_left_y_mod = dest->top_left.y % 8;
     
     for (int x = page_start ;  x <= page_end; x++)
     {
        row_number_start_page = 0;
        row_number_stop_page  = LCD_SEGMENTS - 1;
       
        sendSpiChar(0xb0 | x, DEV_LCD); // set page no
        sendSpiChar(0x10 | (i >> 4), DEV_LCD);  // set column no.
        sendSpiChar(0x00 | (i & 0x0f), DEV_LCD);// set column no.
        P1OUT |= 0x08; // lcd a0 pin = 1
        unsigned char graph_orig     = graphics_ptr[x][i];
        unsigned char graph_new_full = graph_orig;
        
        if ( x == page_start) // check to see if we need to move the row_number_start_page to clip copying
        {
           row_number_start_page = dest_top_left_y_mod % 8;
        }
        
        if (x == page_end) // check to see if we need to move the row_number_stop_page to clip copying
        {
           row_number_stop_page  = dest->bottom_right.y % 8;
        }
        
        // form copy mask
        unsigned char copy_mask = 0;
        for (int j = row_number_start_page; j <= row_number_stop_page; j++)
        {
           copy_mask |= 1 << j;
        }

        graph_new_full &= ~copy_mask; // mark zeros where we will place the new box
         
        // find differential within page from source to destination
        int differential = dest_top_left_y_mod - source->top_left.y % 8;
      
        // page from which we source the 0th vertical pixel in the dest page
        unsigned char from_page = (source->top_left.y + 8 * (x - page_start) - ( dest_top_left_y_mod - row_number_start_page) ) / 8;
        
        // form the page we get the new graphics from
        unsigned char box_graphics = graphics_ptr[from_page][i];
        
        // might need to flip what is in the if statement with whats in the else if
        // shift to take into account the differential
        if (differential > 0)
        {
           box_graphics  = box_graphics << differential;
           box_graphics |= graphics_ptr[from_page - 1][i] >> (8 - differential);
        }
        else
        {
           box_graphics  = box_graphics >> (-1 * differential ) ;
           box_graphics |= graphics_ptr[from_page + 1][i] << (8 + differential);
        }
        
        box_graphics &= copy_mask;
        graph_new_full |= box_graphics;
        
        // combine box graphics with new graphics
        
        sendSpiChar(graph_new_full, DEV_LCD);
        P1OUT &= ~0x08; // lcd a0 pin = 0
     }
  }
  
#endif /* GRAPHICS_ENABLED */
}

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
  writeBoxFromGraphics(&full_box, &full_box);
 
  
  while (1)
  {
     
      checkRadioSignal();
    
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
     
      
      // enable to test flipping every cycle
      //lcd_state.feed_enabled = !lcd_state.feed_enabled;
      //P1OUT = P1OUT ^ 0x06;
  }
   
  return 0;
}

