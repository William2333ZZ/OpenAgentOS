#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

struct agentos_platform {
    const char *name;
    const char *arch;
    unsigned long uart_mmio;
    unsigned long virtio_mmio_base;
    unsigned long virtio_mmio_stride;
    int virtio_mmio_count;
    unsigned long ram_base;
    unsigned long heap_start;
    unsigned long kernel_load;
};

void platform_init(void);
const struct agentos_platform *platform_get(void);
const char *platform_name(void);

unsigned long platform_uart_mmio(void);
unsigned long platform_virtio_mmio_base(void);
unsigned long platform_virtio_mmio_stride(void);
int platform_virtio_mmio_count(void);
unsigned long platform_heap_start(void);

void boot_banner(const char *demo_title);

#endif
