/* OneOS-ARM Terminal UI */

#ifndef TERMINAL_H
#define TERMINAL_H

/* Define basic types for bare-metal */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

/* Terminal dimensions (1024/16 = 64 cols, 768/16 = 48 rows) */
#define TERM_WIDTH  64
#define TERM_HEIGHT 48

/* Terminal state */
typedef struct {
    char buffer[TERM_WIDTH * TERM_HEIGHT];
    char shadow_buffer[TERM_WIDTH * TERM_HEIGHT];
    int cursor_x;
    int cursor_y;
    int old_cursor_x;
    int old_cursor_y;
    int mouse_x;
    int mouse_y;
} terminal_t;

extern terminal_t terminal;

/* Initialize terminal */
void terminal_init(void);

/* Print character at cursor position */
void terminal_putchar(char c);

/* Print string */
void terminal_puts(const char *s);

/* Print hex number */
void terminal_puthex(uint32_t val);

/* Clear terminal */
void terminal_clear(void);

/* Render terminal to output */
void terminal_render(void);

/* Update mouse position */
void terminal_set_mouse(int x, int y);

/* Handle keyboard input */
void terminal_handle_input(uint8_t key);

#endif
