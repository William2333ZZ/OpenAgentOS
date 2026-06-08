#include "uart.h"
#include "spinlock.h"
#include "../include/platform.h"
#include "../include/agentos.h"

#ifdef PLATFORM_X86_64_PC

static spinlock_t uart_lock;
static int uart_lock_inited;

static void uart_lock_init_once(void) {
    if (!uart_lock_inited) {
        spinlock_init(&uart_lock);
        uart_lock_inited = 1;
    }
}

static void uart_outb(unsigned char val, unsigned short port) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static unsigned char uart_inb(unsigned short port) {
    unsigned char ret;

    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void uart_init(void) {}

void uart_putc(char c) {
    unsigned short port = (unsigned short)platform_uart_mmio();

    uart_lock_init_once();
    spinlock_acquire(&uart_lock);
    while ((uart_inb((unsigned short)(port + 5)) & 0x20u) == 0)
        ;
    uart_outb((unsigned char)c, port);
    spinlock_release(&uart_lock);
}

void uart_puts(const char *s) {
    while (*s)
        uart_putc(*s++);
}

void uart_puthex(unsigned long val) {
    const char *hex = "0123456789abcdef";

    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putc(hex[(val >> i) & 0xf]);
}

int uart_poll_char(char *out) {
    unsigned short port = (unsigned short)platform_uart_mmio();

    if (!out)
        return 0;
    uart_lock_init_once();
    spinlock_acquire(&uart_lock);
    if ((uart_inb((unsigned short)(port + 5)) & 1u) == 0) {
        spinlock_release(&uart_lock);
        return 0;
    }
    *out = (char)uart_inb(port);
    spinlock_release(&uart_lock);
    return 1;
}

int uart_getline(char *buf, int maxlen) {
    int pos = 0;
    char c;

    if (!buf || maxlen <= 0)
        return EINVAL;

    while (pos < maxlen - 1) {
        while (!uart_poll_char(&c))
            ;
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            uart_putc('\n');
            return pos;
        }
        if (c == 127 || c == 8) {
            if (pos > 0) {
                pos--;
                uart_putc('\b');
                uart_putc(' ');
                uart_putc('\b');
            }
            continue;
        }
        buf[pos++] = c;
        uart_putc(c);
    }
    buf[pos] = '\0';
    return pos;
}

#else

#define THR(off)  (*(volatile unsigned char *)(platform_uart_mmio() + (off)))
#define LSR(off)  (*(volatile unsigned char *)(platform_uart_mmio() + (off)))

static spinlock_t uart_lock;
static int uart_lock_inited;

static void uart_lock_init_once(void) {
    if (!uart_lock_inited) {
        spinlock_init(&uart_lock);
        uart_lock_inited = 1;
    }
}

void uart_init(void) {}

void uart_putc(char c) {
    uart_lock_init_once();
    spinlock_acquire(&uart_lock);
    while ((LSR(5) & (1 << 5)) == 0)
        ;
    THR(0) = (unsigned char)c;
    spinlock_release(&uart_lock);
}

void uart_puts(const char *s) {
    while (*s)
        uart_putc(*s++);
}

void uart_puthex(unsigned long val) {
    const char *hex = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        uart_putc(hex[(val >> i) & 0xf]);
}

int uart_poll_char(char *out) {
    unsigned char lsr;

    if (!out)
        return 0;
    uart_lock_init_once();
    spinlock_acquire(&uart_lock);
    lsr = LSR(5);
    if ((lsr & 1u) == 0) {
        spinlock_release(&uart_lock);
        return 0;
    }
    *out = (char)THR(0);
    spinlock_release(&uart_lock);
    return 1;
}

int uart_getline(char *buf, int maxlen) {
    int pos = 0;
    char c;

    if (!buf || maxlen <= 0)
        return EINVAL;

    uart_lock_init_once();
    while (pos < maxlen - 1) {
        while (!uart_poll_char(&c)) {
            /* busy-wait; console REPL runs in service agent */
        }
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            uart_putc('\n');
            return pos;
        }
        if (c == 127 || c == 8) {
            if (pos > 0) {
                pos--;
                uart_putc('\b');
                uart_putc(' ');
                uart_putc('\b');
            }
            continue;
        }
        buf[pos++] = c;
        uart_putc(c);
    }
    buf[pos] = '\0';
    return pos;
}

#endif
