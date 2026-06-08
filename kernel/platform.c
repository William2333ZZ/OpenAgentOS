#include "../include/platform.h"
#include "uart.h"
#include "printf.h"

extern const struct agentos_platform platform_riscv_virt;
extern const struct agentos_platform platform_x86_pc;

static const struct agentos_platform *current;

void platform_init(void) {
#ifdef PLATFORM_X86_64_PC
    current = &platform_x86_pc;
#else
    current = &platform_riscv_virt;
#endif
}

const struct agentos_platform *platform_get(void) {
    if (!current)
        platform_init();
    return current;
}

const char *platform_name(void) {
    return platform_get()->name;
}

unsigned long platform_uart_mmio(void) {
    return platform_get()->uart_mmio;
}

unsigned long platform_virtio_mmio_base(void) {
    return platform_get()->virtio_mmio_base;
}

unsigned long platform_virtio_mmio_stride(void) {
    return platform_get()->virtio_mmio_stride;
}

int platform_virtio_mmio_count(void) {
    return platform_get()->virtio_mmio_count;
}

unsigned long platform_heap_start(void) {
    return platform_get()->heap_start;
}

void boot_banner(const char *demo_title) {
    const struct agentos_platform *p = platform_get();

#ifndef AGENTOS_VERSION
#define AGENTOS_VERSION "v4.0"
#endif
    uart_puts("\n");
    uart_puts("========================================\n");
    kprintf("  OpenAgentOS %s — %s\n", AGENTOS_VERSION, demo_title);
    kprintf("  platform: %s (%s)\n", p->name, p->arch);
    uart_puts("========================================\n");
}
