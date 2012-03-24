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


#include "graphics.h"

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
  
#endif
}