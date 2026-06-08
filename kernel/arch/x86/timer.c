#include "timer.h"

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xa0
#define PIC2_DATA 0xa1
#define PIC_EOI 0x20

#define TIMER_HZ 100

static volatile unsigned long timer_ticks;

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void timer_pic_eoi(void) {
    if (inb(PIC1_CMD) & 0x80)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void timer_ack(void) {
    timer_ticks++;
    timer_pic_eoi();
}

void timer_init(void) {
    unsigned long divisor = 1193182UL / TIMER_HZ;

    outb(PIC1_CMD, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_CMD, 0x11);
    outb(PIC2_DATA, 0x28);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);

    /* Mask all PIC lines until the scheduler arms the timer for user agents. */
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);

    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (unsigned char)(divisor & 0xff));
    outb(PIT_CH0, (unsigned char)((divisor >> 8) & 0xff));
}

unsigned long timer_now(void) {
    return timer_ticks;
}

void timer_irq_unmask(void) {
    outb(PIC1_DATA, 0xfe);
}
