/* OneOS-ARM Keyboard Driver Implementation */

#include "keyboard.h"
#include "uart.h"

/* PL011 UART Register Offsets */
#define UART_DR     0x00    /* Data Register */
#define UART_FR     0x18    /* Flag Register */

/* Flag Register bits */
#define UART_FR_RXFE (1 << 4)   /* Receive FIFO empty */
#define UART_FR_RXFF (1 << 6)   /* Receive FIFO full */

/* Base address of UART0 */
#define UART0_BASE 0x09000000

void keyboard_init(void)
{
    /* Keyboard uses same UART as console, already initialized */
}

int keyboard_available(void)
{
    volatile unsigned int *uart = (volatile unsigned int *)UART0_BASE;
    return !(uart[UART_FR / 4] & UART_FR_RXFE);
}

uint8_t keyboard_read(void)
{
    while (!keyboard_available()) {
        /* Busy wait */
    }
    return keyboard_read_nonblock();
}

uint8_t keyboard_read_nonblock(void)
{
    volatile unsigned int *uart = (volatile unsigned int *)UART0_BASE;

    if (uart[UART_FR / 4] & UART_FR_RXFE) {
        return 0;  /* No data available */
    }

    return (uint8_t)uart[UART_DR / 4];
}

void keyboard_flush(void)
{
    while (keyboard_available()) {
        keyboard_read_nonblock();
    }
}
