#ifndef SPI_H
#define SPI_H

typedef enum { DEV_RADIO, DEV_LCD } spi_device_t;

void setupSpi();
unsigned char sendSpiChar(unsigned char c, spi_device_t dev);
void sendSpiStringLen(unsigned char * sz, int len, spi_device_t dev);

#endif /* SPI_H */