/* OneOS-ARM Framebuffer Driver */

#ifndef FB_H
#define FB_H

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int bpp;      /* Bits per pixel */
    unsigned char *buffer;
} framebuffer_t;

/* Initialize framebuffer */
void fb_init(void);

/* Get framebuffer info */
framebuffer_t* fb_get(void);

/* Clear screen with color */
void fb_clear(unsigned int color);

/* Draw pixel at (x, y) */
void fb_draw_pixel(int x, int y, unsigned int color);

/* Draw filled rectangle */
void fb_draw_rect(int x, int y, int width, int height, unsigned int color);

/* Draw horizontal line */
void fb_draw_hline(int x, int y, int length, unsigned int color);

/* Draw vertical line */
void fb_draw_vline(int x, int y, int length, unsigned int color);

/* Color definitions (32-bit BGRX8888) */
#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_RED       0x00FF0000
#define COLOR_GREEN     0x0000FF00
#define COLOR_BLUE      0x000000FF
#define COLOR_CYAN      0x0000FFFF
#define COLOR_MAGENTA   0x00FF00FF
#define COLOR_YELLOW    0x00FFFF00
#define COLOR_GRAY      0x00808080
#define COLOR_LIGHT_GRAY 0x00C0C0C0
#define COLOR_DARK_BLUE 0x00000060

/* Default framebuffer physical base (may vary by QEMU/device) */
#define FB_BASE 0x3eff0000

#endif
