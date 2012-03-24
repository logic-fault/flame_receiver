#include "spi.h"
#include "io430.h"

void setupSpi()
{
   // setup SPI
   USICKCTL = 0x48; // main clock, divide by 128
   USICTL0  = 0xEA; //11101010 spi outputs enabled; not held in reset state
   USICTL1 |= 0x80; // data captured on leading clock edge
   //USICTL0 &= ~0x01; // enable spi
   P1OUT &= ~0x10;
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