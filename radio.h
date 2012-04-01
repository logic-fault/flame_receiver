#ifndef RADIO_H
#define RADIO_H

typedef enum { SIG_NONE = 0,       // no signal received
               SIG_FIRE_START = 0x07, // initiate charge or fire sequence
               SIG_FIRE_ABORT = 0x09, // stop firing sequence, if in progress
               SIG_ARM        = 0x05,        // allow firing
               SIG_DISARM     = 0x03      // disallow firing
             } radio_signal_t;

typedef struct
{
  unsigned char fifo[2];
  unsigned char status[2];
} radio_status_t;

void setupRadio();
void getRadioStatus(radio_status_t * stat);
radio_signal_t checkRadioSignal();

#endif /* RADIO_H */