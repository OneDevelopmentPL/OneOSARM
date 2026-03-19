/* OneOS-ARM Terminal UI Implementation */

#include "terminal.h"
#include "graphics.h"
#include "fb.h"
#include "fb.h"

/* Global terminal state */
terminal_t terminal;

/* Terminal colors */
#define TERM_FG_COLOR COLOR_WHITE
#define TERM_BG_COLOR COLOR_BLACK

/* Character size in pixels (16x16 = 2x scaled 8x8 font) */
#define CHAR_W 16
#define CHAR_H 16

void terminal_init(void)
{
    /* Initialize the framebuffer system (this calls gpu_init inside) */
    fb_init();
    
    terminal.cursor_x = 0;
    terminal.cursor_y = 0;
    terminal.old_cursor_x = 0;
    terminal.old_cursor_y = 0;
    terminal.mouse_x = 40;
    terminal.mouse_y = 12;
    
    /* Initialize buffer */
    for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
        terminal.buffer[i] = ' ';
        terminal.shadow_buffer[i] = '\0';
    }
}

void terminal_clear(void)
{
    for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
        terminal.buffer[i] = ' ';
        terminal.shadow_buffer[i] = '\0';
    }
    terminal.cursor_x = 0;
    terminal.cursor_y = 0;
    fb_clear(TERM_BG_COLOR);
}

void terminal_scroll(void)
{
    /* Move everything up one line */
    for (int y = 0; y < TERM_HEIGHT - 1; y++) {
        for (int x = 0; x < TERM_WIDTH; x++) {
            terminal.buffer[y * TERM_WIDTH + x] = terminal.buffer[(y + 1) * TERM_WIDTH + x];
        }
    }
    /* Clear last line */
    for (int x = 0; x < TERM_WIDTH; x++) {
        terminal.buffer[(TERM_HEIGHT - 1) * TERM_WIDTH + x] = ' ';
    }
    /* Invalidate shadow array to force redraw next frame */
    for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
        terminal.shadow_buffer[i] = '\0';
    }
    terminal.cursor_y = TERM_HEIGHT - 1;
}

void terminal_putchar(char c)
{
    if (c == '\n') {
        terminal.cursor_y++;
        terminal.cursor_x = 0;
        if (terminal.cursor_y >= TERM_HEIGHT) {
            terminal_scroll();
        }
    } else if (c == '\r') {
        terminal.cursor_x = 0;
    } else {
        if (terminal.cursor_x >= TERM_WIDTH) {
            terminal.cursor_x = 0;
            terminal.cursor_y++;
        }
        if (terminal.cursor_y >= TERM_HEIGHT) {
            terminal_scroll();
        }
        
        int pos = terminal.cursor_y * TERM_WIDTH + terminal.cursor_x;
        terminal.buffer[pos] = c;
        terminal.cursor_x++;
    }
}

void terminal_puts(const char *s)
{
    while (*s) {
        terminal_putchar(*s);
        s++;
    }
}

void terminal_puthex(uint32_t n)
{
    char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        terminal_putchar(hex[(n >> (i * 4)) & 0xF]);
    }
}

void terminal_render(void)
{
    /* Clear old cursor if it moved */
    if (terminal.old_cursor_x != terminal.cursor_x || terminal.old_cursor_y != terminal.cursor_y) {
        int old_pos = terminal.old_cursor_y * TERM_WIDTH + terminal.old_cursor_x;
        if (old_pos >= 0 && old_pos < TERM_WIDTH * TERM_HEIGHT) {
            terminal.shadow_buffer[old_pos] = '\0'; /* Force text redraw here */
        }
    }
    
    /* Draw each character at 16x16 scale if it changed */
    for (int y = 0; y < TERM_HEIGHT; y++) {
        for (int x = 0; x < TERM_WIDTH; x++) {
            int pos = y * TERM_WIDTH + x;
            char c = terminal.buffer[pos];
            if (c != terminal.shadow_buffer[pos]) {
                gfx_draw_char(x * CHAR_W, y * CHAR_H, c, TERM_FG_COLOR, TERM_BG_COLOR);
                terminal.shadow_buffer[pos] = c;
            }
        }
    }
    
    /* Draw cursor */
    fb_draw_rect(terminal.cursor_x * CHAR_W, terminal.cursor_y * CHAR_H, CHAR_W, CHAR_H, COLOR_LIGHT_GRAY);
    terminal.old_cursor_x = terminal.cursor_x;
    terminal.old_cursor_y = terminal.cursor_y;

    /* Present final frame to screen */
    fb_present();
}

void terminal_set_mouse(int x, int y)
{
    terminal.mouse_x = x;
    terminal.mouse_y = y;
}

void terminal_handle_input(uint8_t key)
{
    if (key >= 32 && key <= 126) {
        terminal_putchar((char)key);
    } else if (key == 0x08) {  /* Backspace */
        if (terminal.cursor_x > 0) {
            terminal.cursor_x--;
            int pos = terminal.cursor_y * TERM_WIDTH + terminal.cursor_x;
            terminal.buffer[pos] = ' ';
        }
    } else if (key == 0x0D) {  /* Enter */
        terminal_putchar('\n');
    }
}
