#include "mem.h"
#include "spinlock.h"
#include "../include/platform.h"

#define HEAP_SIZE  (2 * 1024 * 1024)

static unsigned char *heap_ptr;
static unsigned char *heap_end;
static spinlock_t mem_lock;
static int mem_lock_inited;

static void mem_lock_init_once(void) {
    if (!mem_lock_inited) {
        spinlock_init(&mem_lock);
        mem_lock_inited = 1;
    }
}

void mem_init(void) {
    heap_ptr = (unsigned char *)platform_heap_start();
    heap_end = heap_ptr + HEAP_SIZE;
}

void *kalloc(unsigned long size) {
    void *p;

    mem_lock_init_once();
    size = (size + 15) & ~15UL;
    spinlock_acquire(&mem_lock);
    if (heap_ptr + size > heap_end) {
        spinlock_release(&mem_lock);
        return 0;
    }
    p = heap_ptr;
    heap_ptr += size;
    spinlock_release(&mem_lock);
    return p;
}

void *kalloc_page(void) {
    unsigned long p;
    void *out;

    mem_lock_init_once();
    spinlock_acquire(&mem_lock);
    p = (unsigned long)heap_ptr;
    p = (p + 4096UL - 1) & ~(4096UL - 1);
    if (p + 4096UL > (unsigned long)heap_end) {
        spinlock_release(&mem_lock);
        return 0;
    }
    out = (void *)p;
    heap_ptr = (unsigned char *)(p + 4096UL);
    spinlock_release(&mem_lock);
    return out;
}

void kfree(void *ptr) {
    (void)ptr;
}
