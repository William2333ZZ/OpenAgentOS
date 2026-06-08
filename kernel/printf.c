#include "printf.h"
#include "uart.h"

static void print_uint(unsigned long n) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        uart_putc('0');
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        uart_putc(buf[--i]);
}

static void print_int(long n) {
    if (n < 0) {
        uart_putc('-');
        print_uint((unsigned long)(-n));
    } else {
        print_uint((unsigned long)n);
    }
}

int kprintf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;
        switch (*p) {
        case 's':
            uart_puts(__builtin_va_arg(ap, const char *));
            break;
        case 'd':
            print_int(__builtin_va_arg(ap, int));
            break;
        case 'u':
            print_uint(__builtin_va_arg(ap, unsigned int));
            break;
        case 'l':
            if (p[1] == 'x') {
                p++;
                uart_puthex(__builtin_va_arg(ap, unsigned long));
            } else {
                print_uint(__builtin_va_arg(ap, unsigned long));
            }
            break;
        case 'x':
            uart_puthex(__builtin_va_arg(ap, unsigned int));
            break;
        case 'c':
            uart_putc((char)__builtin_va_arg(ap, int));
            break;
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('?');
            break;
        }
    }
    __builtin_va_end(ap);
    return 0;
}
