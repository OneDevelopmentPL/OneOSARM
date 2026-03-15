/* OneOS-ARM Keyboard Driver */

#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Define basic types for bare-metal */
typedef unsigned char uint8_t;

/* Key codes (ASCII + special keys) */
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0D
#define KEY_ESC         0x1B
#define KEY_SPACE       0x20

#define KEY_UP          0x80
#define KEY_DOWN        0x81
#define KEY_LEFT        0x82
#define KEY_RIGHT       0x83
#define KEY_F1          0x84
#define KEY_F2          0x85
#define KEY_F3          0x86
#define KEY_F4          0x87
#define KEY_F5          0x88
#define KEY_F6          0x89
#define KEY_F7          0x8A
#define KEY_F8          0x8B
#define KEY_F9          0x8C
#define KEY_F10         0x8D
#define KEY_F11         0x8E
#define KEY_F12         0x8F

/* Initialize keyboard */
void keyboard_init(void);

/* Check if key is available */
int keyboard_available(void);

/* Read key (blocking) */
uint8_t keyboard_read(void);

/* Read key (non-blocking, returns 0 if no key) */
uint8_t keyboard_read_nonblock(void);

/* Flush keyboard buffer */
void keyboard_flush(void);

#endif
