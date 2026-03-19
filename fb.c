/* OneOS-ARM Framebuffer Driver Implementation */

#include "fb.h"
#include "gpu.h"

#define FB_WIDTH    1024
#define FB_HEIGHT   768
#define FB_PITCH    (FB_WIDTH * 4)  /* 4 bytes per pixel at 32bpp */
#define FB_BPP      32              /* 32-bit color (BGRX8888) */

static framebuffer_t fb;
static unsigned char *frontbuffer = 0;
static unsigned int backbuffer[FB_WIDTH * FB_HEIGHT] __attribute__((aligned(16)));
static int use_backbuffer = 0;

void fb_init(void)
{
    gpu_init();
    
    fb.width = FB_WIDTH;
    fb.height = FB_HEIGHT;
    fb.pitch = FB_PITCH;
    fb.bpp = FB_BPP;
    frontbuffer = gpu_get_framebuffer();
    fb.buffer = frontbuffer;
    use_backbuffer = 0;
    
    /* Clear screen directly */
    fb_clear(COLOR_BLACK);
}

framebuffer_t* fb_get(void)
{
    return &fb;
}

void fb_clear(unsigned int color)
{
    volatile unsigned int *vram = (volatile unsigned int *)fb.buffer;
    
    for (int i = 0; i < (int)(fb.width * fb.height); i++) {
        vram[i] = color;
    }
}

void fb_draw_pixel(int x, int y, unsigned int color)
{
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) {
        return;  /* Out of bounds */
    }
    
    volatile unsigned int *vram = (volatile unsigned int *)fb.buffer;
    int offset = y * (int)fb.width + x;
    vram[offset] = color;
}

void fb_draw_rect(int x, int y, int width, int height, unsigned int color)
{
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            fb_draw_pixel(x + col, y + row, color);
        }
    }
}

void fb_draw_hline(int x, int y, int length, unsigned int color)
{
    for (int i = 0; i < length; i++) {
        fb_draw_pixel(x + i, y, color);
    }
}

void fb_draw_vline(int x, int y, int length, unsigned int color)
{
    for (int i = 0; i < length; i++) {
        fb_draw_pixel(x, y + i, color);
    }
}

void fb_present(void)
{
    if (!frontbuffer || !use_backbuffer) return;
    volatile unsigned int *dst = (volatile unsigned int *)frontbuffer;
    unsigned int *src = (unsigned int *)fb.buffer;
    int pixels = (int)(fb.width * fb.height);
    for (int i = 0; i < pixels; i++) {
        dst[i] = src[i];
    }
}

void fb_use_backbuffer(int enable)
{
    if (!frontbuffer) return;
    use_backbuffer = enable ? 1 : 0;
    fb.buffer = use_backbuffer ? (unsigned char *)backbuffer : frontbuffer;
    if (use_backbuffer) {
        fb_clear(COLOR_BLACK);
        fb_present();
    }
}
