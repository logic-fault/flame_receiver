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

#define P1_FEED_VALVE 4
#define P1_RELEASE_VALVE 2
#define HALF_SECONDS_COUNT 0xf424

int timer_iter;

typedef enum {STATE_DISARMED = 0,                  // disarmed, possibly charged
              STATE_UNCHARGED,                 // armed and uncharged
              STATE_CHARGING,                  // armed and charging
              STATE_CHARGED,                   // armed and charged
              STATE_FIRING_REQUESTED,          // armed and firing on next loop
              STATE_FIRING_WAITING_FOR_CHARGE, // armed, charging before firing
              STATE_FIRING,                    // armed and firing
             } system_state_t;


volatile system_state_t system_state = STATE_DISARMED;
volatile system_state_t * sp = &system_state;

lcd_state_t lcd_state = { 0, 1, 1 };

void setSystemState(system_state_t state)
{
  system_state = state;
}

system_state_t getSystemState()
{
  return system_state;
}

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

void solenoid_timer_enable(unsigned int time, unsigned int numTimes) // time in 8 microsecond intervals.  0xffff, 1 = 0.5s
{
  __bic_SR_register(GIE); // no interrupts
  // setup solenoid timer
  // ACLK, /1, Up Mode, ISRs enabled
  TACTL   = 0x0112; 
  TACCTL0 = 0x0010 | 0x00c0; // CCIE enabled, divide ACLK by 8
  TAR     = 0x00;
  TACCR0  = time; // for test, count up to 
  timer_iter = numTimes;
  __bis_SR_register(GIE); // interrupt enabled
}


#pragma vector = PORT1_VECTOR, PORT2_VECTOR, USI_VECTOR, ADC10_VECTOR, TIMERA1_VECTOR, WDT_VECTOR, NMI_VECTOR
__interrupt void default_isr(void)
{
  
  // don't do anything
  int y = 1;
  
  return;
}

#pragma vector=TIMERA0_VECTOR
__interrupt void solenoid_timer_isr(void)
{
  
  
  __bic_SR_register(GIE); // no interrupts
 
  timer_iter--;
  for (int i = 0; i < 10; i++);
  if (timer_iter > 0)
  {
     TACTL &= ~0x0030; // halt timer
     TACTL |= TACLR;
     TACTL   = 0x0112; 
     TACCTL0 = 0x0010 | 0x00c0; // CCIE enabled, divide ACLK by 8
     TAR = 0x00;
     __bis_SR_register(GIE); // re-enable interrupts
     return;
  }
  
  TACTL &= ~0x0030; // halt timer
  TACTL |= TACLR; // clear the timer

  closeValves();
  
  system_state_t system_state = getSystemState();
  
  // we were charging, now valves closed and tank pressurized
  if (system_state == STATE_CHARGING)
  {
    setSystemState(STATE_CHARGED);
  }
  
  
  // see if we were charging and waiting to fire
  else if (system_state == STATE_FIRING_WAITING_FOR_CHARGE)
  {
    setSystemState(STATE_FIRING_REQUESTED);
  }
  
  else if (system_state == STATE_FIRING)
  {
     setSystemState(STATE_UNCHARGED);
  }
  
  __bis_SR_register(GIE); // re-enable interrupts
  
  return;
}

void chargeFillTank()
{
  if (getSystemState() == STATE_DISARMED)
    return;
  setSystemState(STATE_CHARGING);
  
  closeValves();
  
  // start charging
  openFeedValve();
  
  solenoid_timer_enable(HALF_SECONDS_COUNT, 4);
}

void releaseFillTank()
{
   if (getSystemState() != STATE_FIRING_REQUESTED)
      return;
   
   setSystemState(STATE_FIRING);
   
   closeValves();
   
   // start firing
   openReleaseValve();
   
   solenoid_timer_enable(HALF_SECONDS_COUNT, 1);
}

void handleRadioSignal(radio_signal_t sig)
{
  
  system_state_t system_state = getSystemState();
  
  switch (sig)
  {
  case SIG_FIRE_START:          // ensure system charged, close valves, open release for X seconds
    if (system_state != STATE_CHARGED)
    {
      if (system_state != STATE_CHARGING)
         chargeFillTank();
      setSystemState(STATE_FIRING_WAITING_FOR_CHARGE);
    }
    else
      system_state = STATE_FIRING_REQUESTED;
    break;
  case SIG_FIRE_ABORT:
      closeValves();
      if (system_state != STATE_CHARGED)
         setSystemState(STATE_UNCHARGED);
    break;
  case SIG_ARM:
    // open fill tank, close after specified time
    setSystemState(STATE_UNCHARGED);
    lcd_state.armed_enabled = 1;
    break;
  case SIG_DISARM:
    closeValves();
    setSystemState(STATE_DISARMED);
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

int reset_count; 
 
int main( void )
{
  
  reset_count++;
  
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
  
  
  for (int i = 0; i < 100; i++);
  
  
  setupSpi();
  setupRadio();

  // initialize the lcd
#ifdef GRAPHICS_ENABLED  
  sendSpiStringLen(lcdInit, 14, DEV_LCD);
  sendSpiChar(0xb0, DEV_LCD); // page 0
  // write a block to ram
  writeBoxFromGraphics(&full_box, &full_box);
#endif
  
  __bis_SR_register(GIE); // general interrupt enable
  
  handleRadioSignal(SIG_ARM);
  
  system_state_t last_state;
  lcd_state_t last_lcd_state;
  
  int j;
  
  while (1)
  {
     
      //handleRadioSignal(checkRadioSignal());
      
       last_state = getSystemState();
    
      switch(getSystemState())
      {
      case STATE_UNCHARGED:
        lcd_state.feed_enabled = 0;
        lcd_state.release_enabled = 0;
        lcd_state.armed_enabled = 1;
        chargeFillTank();
        break;
      case STATE_FIRING_REQUESTED:
        lcd_state.feed_enabled = 0;
        lcd_state.release_enabled = 0;
        lcd_state.armed_enabled = 1;
        releaseFillTank();
        break;
      case STATE_CHARGED:
        lcd_state.feed_enabled = 0;
        lcd_state.release_enabled = 0;
        lcd_state.armed_enabled = 1;
        break;
      case STATE_DISARMED:
        lcd_state.feed_enabled = 0;
        lcd_state.release_enabled = 0;
        lcd_state.armed_enabled = 0;
        break;
      case STATE_CHARGING:
        lcd_state.feed_enabled = 1;
        lcd_state.release_enabled = 0;
        lcd_state.armed_enabled = 1;
        break;
      case STATE_FIRING:
        lcd_state.feed_enabled = 0;
        lcd_state.release_enabled = 1;
        lcd_state.armed_enabled = 1;
        break;
      case STATE_FIRING_WAITING_FOR_CHARGE:
        // intentional fall-through
        break;
      default:
        // nothing to do
        break;
      }

      
      
#ifdef GRAPHICS_ENABLED
      
      if (lcd_state.armed_enabled == last_lcd_state.armed_enabled
      else if (lcd_state.armed_enabled)
      {
         //writeBoxFromGraphics(feed_box, );
      }
    
      if (lcd_state.feed_enabled == last_lcd_state.feed_enabled);
      else if (lcd_state.feed_enabled)
         writeBoxFromGraphics( &release_on_box, &feed_off_box );
      else
         writeBoxFromGraphics( &feed_off_box, &feed_off_box);
      
      if (lcd_state.release_enabled == last_lcd_state.release_enabled);     
      else if (!lcd_state.release_enabled)
         writeBoxFromGraphics( &feed_off_box, &release_on_box );
      else
         writeBoxFromGraphics( &release_on_box, &release_on_box);
      
      last_lcd_state = lcd_state;
      
#endif    
      
      // enable to test flipping every cycle
      //lcd_state.feed_enabled = !lcd_state.feed_enabled;
      //P1OUT = P1OUT ^ 0x06;
  }
   
  return 0;
}

