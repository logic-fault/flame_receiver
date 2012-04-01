/******************************************************************************
 *   Copyright (C) 2012,  Brock Anderson
 *   brock.wright.anderson@gmail.com
 *
 *   This file is part of psirens_of_entropy.
 *
 *   Foobar is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Foobar is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with psirens_of_entropy.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/


#include "radio.h"
#include "spi.h"
#include "io430.h"

radio_status_t radio_status;

// for latest sent message
const unsigned char RADIO_MSG_BUFFER_SIZE = 4;
unsigned char radio_msg[4]; // should use the constant above but compiler bitches
const unsigned char RADIO_SYNC_WORD = 0x77a5;
const unsigned char MY_RADIO_NUMBER    = 0x01;  // can be any number gt. 0x0f
const unsigned char BROADCAST_RADIO_NUMBER = 0x0f;

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
  {
     radio_msg[msg_write_ptr] = radio_status.fifo[0];
     
     // invert odd signal numbers
     if (msg_write_ptr & 0x01 != 0x00)
       radio_msg[msg_write_ptr] = ~radio_msg[msg_write_ptr];
     
     // check each message is the same
     if (msg_write_ptr > 0 && radio_msg[msg_write_ptr] != radio_msg[0])
       msg_write_ptr = 0;
     else
       msg_write_ptr++;
  }
  else
    return SIG_NONE;
  
  // see if the message was addressed to either us or broadcasted
  unsigned char radio_num_rx = (radio_msg[0] & 0xf0) >> 4;
  if (radio_num_rx != MY_RADIO_NUMBER && radio_num_rx != BROADCAST_RADIO_NUMBER)
    return SIG_NONE
  
   if (msg_write_ptr == RADIO_MSG_BUFFER_SIZE)
   {
      msg_write_ptr = 0;
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