/* OneOS-ARM Graphics Library */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "fb.h"

/* Font: 8x8 bitmap font (ASCII) */
extern const unsigned char font8x8[128][8];

/* Draw character at position */
void gfx_draw_char(int x, int y, char c, unsigned int fg_color, unsigned int bg_color);

/* Draw string */
void gfx_draw_string(int x, int y, const char *str, unsigned int fg_color, unsigned int bg_color);

/* Draw a filled rectangle with border */
void gfx_draw_window(int x, int y, int width, int height, unsigned int border_color, unsigned int bg_color);

/* Draw button */
void gfx_draw_button(int x, int y, int width, int height, const char *label, unsigned int color);

#endif
