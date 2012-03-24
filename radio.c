#include "radio.h"
#include "spi.h"
#include "io430.h"

radio_status_t radio_status;

// for latest sent message
const unsigned char RADIO_MSG_BUFFER_SIZE = 4;
unsigned char radio_msg[4]; // should use the constant above but compiler bitches
const int RADIO_SYNC_WORD = 0x77a5;

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
  static unsigned char msg_write_ptr = 0;
  
  getRadioStatus(&radio_status);
  if ( (radio_status.status[0] & 0x80) != 0)
     radio_msg[msg_write_ptr++] = radio_status.fifo[0];
  else
    return SIG_NONE;
  
   if (msg_write_ptr == RADIO_MSG_BUFFER_SIZE)
   {
      msg_write_ptr = 0;
      unsigned char message_valid = 1;
      for (int i = 1; i < RADIO_MSG_BUFFER_SIZE; i++)
      {
        // invert every other byte
        if (i & 0x01 != 0x00)
          radio_msg[i] = ~radio_msg[i];
        
        if (radio_msg[i] != radio_msg[0])
          message_valid = 0;
      }
      
      if (message_valid)
        return (radio_signal_t)radio_msg[0];
      
   }
   
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