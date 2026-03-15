/* OneOS-ARM 1 UART Driver
 * PL011 UART for QEMU virt machine
 * UART0 base address: 0x09000000
 */

#include "uart.h"

/* PL011 UART Register Offsets */
#define UART_DR     0x00    /* Data Register */
#define UART_LCRH   0x2C    /* Line Control Register (High) */
#define UART_CR     0x30    /* Control Register */
#define UART_FR     0x18    /* Flag Register */

/* Flag Register bits */
#define UART_FR_TXFE (1 << 7)   /* Transmit FIFO empty */
#define UART_FR_BUSY (1 << 3)   /* UART busy */

/* Base address of UART0 */
#define UART0_BASE 0x09000000

void uart_init(void)
{
    volatile unsigned int *uart = (volatile unsigned int *)UART0_BASE;
    
    /* Disable UART before configuration */
    uart[UART_CR / 4] = 0;
    
    /* Configure line control: 8 bits, no parity, 1 stop bit, FIFO enabled */
    uart[UART_LCRH / 4] = 0x60;  /* 8-bit mode, FIFO enabled */
    
    /* Set baud rate - skip for now, use default */
    
    /* Enable UART: TX and RX */
    uart[UART_CR / 4] = 0x0301;  /* TXE | RXE | UARTEN */
}

void uart_send(unsigned char c)
{
    volatile unsigned int *uart = (volatile unsigned int *)UART0_BASE;
    
    /* Wait until transmit FIFO is not full */
    while (uart[UART_FR / 4] & (1 << 5)) {  /* TXFF - Transmit FIFO Full */
        /* Spin */
    }
    
    /* Send character */
    uart[UART_DR / 4] = c;
}

void uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            uart_send('\r');
        }
        uart_send(*str++);
    }
}

/* Print 64-bit value in hex (no leading 0x) */
void uart_puthex(unsigned long long v)
{
    const char *hex = "0123456789ABCDEF";
    int started = 0;
    for (int i = (sizeof(v) * 8) - 4; i >= 0; i -= 4) {
        unsigned int nibble = (v >> i) & 0xF;
        if (nibble || started || i == 0) {
            uart_send(hex[nibble]);
            started = 1;
        }
    }
}
