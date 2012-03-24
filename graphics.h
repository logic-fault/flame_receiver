#ifndef GRAPHICS_H
#define GRAPHICS_H

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

void writeBoxFromGraphics(const box_t * source, const box_t * dest);

#endif /* GRAPHICS_H */