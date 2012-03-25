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

#include "io430.h"

/*#define GRAPHICS_ENABLED*/

#ifdef GRAPHICS_ENABLED
#include "lcd_graphics_arrays.h"
#endif /* GRAPHICS_ENABLED */

#include "graphics.h"
#include "radio.h"
#include "spi.h"

#define P1_FEED_VALVE 2
#define P1_RELEASE_VALVE 4

typedef enum {STATE_DISARMED,                  // disarmed, possibly charged
              STATE_UNCHARGED,                 // armed and uncharged
              STATE_CHARGING,                  // armed and charging
              STATE_CHARGED,                   // armed and charged
              STATE_FIRING_REQUESTED,          // armed and firing on next loop
              STATE_FIRING_WAITING_FOR_CHARGE, // armed, charging before firing
              STATE_FIRING,                    // armed and firing
             } system_state_t;


system_state_t system_state = STATE_DISARMED;

lcd_state_t lcd_state = { 0, 1, 1 };

void closeValves()
{
  P1OUT &= ~(P1_FEED_VALVE | P1_RELEASE_VALVE);
}

void openFeedValve()
{
  P1OUT |= P1_FEED_VALVE;
}

void openReleaseValve()
{
  P1OUT |= P1_RELEASE_VALVE;
}

void solenoid_timer_enable(unsigned int time)
{
  // setup solenoid timer
  // ACLK, /1, Up Mode, ISRs enabled
  TACTL   = 0x0112; 
  TACCTL0 = 0x0010; // CCIE enabled
  TAR     = 0x00;
  TACCR0  = time; // for test, count up to 
}

#pragma vector=TIMERA0_VECTOR
__interrupt void solenoid_timer_isr(void)
{
  __bic_SR_register(GIE); // no interrupts
  
  TACTL &= ~0x0030; // halt timer
  TACTL |= TACLR; // clear the timer
  
  // TODO: clear interrupt, turnoff valves, change state
  closeValves();
  
  // we were charging, now valves closed and tank pressurized
  if (system_state == STATE_CHARGING)
  {
    system_state = STATE_CHARGED;
  }
  
  
  // see if we were charging and waiting to fire
  else if (system_state == STATE_FIRING_WAITING_FOR_CHARGE)
  {
    system_state = STATE_FIRING_REQUESTED;
    return;
  }
  
  else if (system_state == STATE_FIRING)
  {
     system_state = STATE_UNCHARGED;
  }
  
  __bis_SR_register(GIE); // re-enable interrupts
  
  return;
}

void chargeFillTank()
{
  if (system_state == STATE_DISARMED)
    return;
  system_state = STATE_CHARGING;
  
  closeValves();
  
  // start charging
  openFeedValve();
  
  // TODO: create a timer for stopping charge. When stopped state = STATE_CHARGED
}

void releaseFillTank()
{
   if (system_state != STATE_FIRING_REQUESTED)
      return;
   
   system_state = STATE_FIRING;   
   
   closeValves();
   
   // start firing
   openReleaseValve();
   
   // TODO: create a timer for stopping.  When stopped state = STATE_UNCHARGED
}

void handleRadioSignal(radio_signal_t sig)
{
  switch (sig)
  {
  case SIG_FIRE_START:          // ensure system charged, close valves, open release for X seconds
    if (system_state != STATE_CHARGED)
    {
      if (system_state != STATE_CHARGING)
         chargeFillTank();
      system_state = STATE_FIRING_WAITING_FOR_CHARGE;
    }
    else
      system_state = STATE_FIRING_REQUESTED;
    break;
  case SIG_FIRE_ABORT:
      closeValves();
      if (system_state != STATE_CHARGED)
         system_state = STATE_UNCHARGED;
    break;
  case SIG_ARM:
    // open fill tank, close after specified time
    system_state = STATE_UNCHARGED;
    lcd_state.armed_enabled = 1;
    break;
  case SIG_DISARM:
    closeValves();
    system_state = STATE_DISARMED;
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
  
  // setup solenoid timer
  // ACLK, /1, Up Mode, ISRs enabled
  TACTL   = 0x0112; 
  TACCTL0 = 0x0010; // CCIE enabled
  TAR     = 0x00;
  TACCR0  = 0x1000; // for test, count up to
  
  __bis_SR_register(GIE); // general interrupt enable
  
  while(TAR < 0x0020);
  
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
      
      switch(system_state)
      {
      case STATE_UNCHARGED:
        chargeFillTank();
        break;
      case STATE_FIRING_REQUESTED:
        releaseFillTank();
        break;
      case STATE_CHARGED:
      case STATE_DISARMED:
      case STATE_CHARGING:
      case STATE_FIRING:
      case STATE_FIRING_WAITING_FOR_CHARGE:
        // intentional fall-through
      default:
        // nothing to do
        break;
      }

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

