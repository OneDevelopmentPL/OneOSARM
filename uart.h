/* OneOS-ARM 1 UART Header */

#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_send(unsigned char c);
void uart_puts(const char *str);
void uart_puthex(unsigned long long v);

#endif
