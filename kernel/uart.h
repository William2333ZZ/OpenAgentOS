#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(unsigned long val);
/* Non-blocking: returns 1 if byte read, 0 if none. */
int uart_poll_char(char *out);
int uart_getline(char *buf, int maxlen);

#endif
