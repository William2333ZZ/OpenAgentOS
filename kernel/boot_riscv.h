#ifndef BOOT_RISCV_H
#define BOOT_RISCV_H

#include "../include/platform.h"
#include "uart.h"
#include "trap.h"

static inline void agentos_boot_banner(const char *demo_title) {
    platform_init();
    uart_init();
    boot_banner(demo_title);
}

static inline void agentos_trap_init(void) {
    trap_init();
}

#endif
