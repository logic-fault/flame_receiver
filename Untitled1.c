
#include "io430.h"


void setupSpi()
{
   // setup SPI
   USICKCTL = 0xE8; // main clock, divide by 128
   USICTL0  = 0xEA; //11101011 spi outputs enabled; held in reset state
   //USICTL0 &= ~0x01; // enable spi
}

void sendSpiChar(unsigned char c)
{
   USISRL = c;     // set the output shift register with our 8 bits
   P1OUT &= ~0x01; // Slave_Select_BAR
   USICNT = 0x08;  // we have 8 bits to send
   while ((USICNT & 0x1f) > 0); // make sure we sent all 8 bits
   P1OUT |= 0x01;  // Slave_Select_BAR
   return;
}

void sendSpiString(char * sz)
{
   while (*sz) sendSpiChar(*sz++);
}




int main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  P1DIR  = 0x01;
  P1OUT  = 0x01; // enable flag for SPI
  
  setupSpi();
  
  sendSpiString("sativa");
  for (int i = 0; i < 100; i++); // burn some time
  sendSpiString("indica");
  
  while (1)
  {
     
  }
  
  return 0;
}

