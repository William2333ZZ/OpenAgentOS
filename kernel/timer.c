#include "timer.h"
#include <stdint.h>

#define TIMER_INTERVAL 500000UL

static uint64_t read_time(void) {
    uint64_t t;
    __asm__ volatile("csrr %0, time" : "=r"(t));
    return t;
}

static void sbi_set_timer(uint64_t when) {
    register unsigned long a0 asm("a0") = when;
    register unsigned long a7 asm("a7") = 0;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}

void timer_ack(void) {
    sbi_set_timer(read_time() + TIMER_INTERVAL);
}

void timer_init(void) {
    unsigned long sie;
    __asm__ volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1UL << 5);
    __asm__ volatile("csrw sie, %0" : : "r"(sie));

    timer_ack();
}

void timer_irq_unmask(void) {
}

unsigned long timer_now(void) {
    return (unsigned long)(read_time() / TIMER_INTERVAL);
}
