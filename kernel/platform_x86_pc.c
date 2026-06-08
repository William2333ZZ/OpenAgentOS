#include "../include/platform.h"

const struct agentos_platform platform_x86_pc = {
    .name = "x86_64-pc",
    .arch = "x86_64",
    .uart_mmio = 0x3f8UL,
    .virtio_mmio_base = 0,
    .virtio_mmio_stride = 0,
    .virtio_mmio_count = 0,
    .ram_base = 0,
    .heap_start = 0x300000UL,
    .kernel_load = 0x100000UL,
};
