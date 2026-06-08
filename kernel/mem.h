#ifndef MEM_H
#define MEM_H

#include <stdint.h>

void mem_init(void);
void *kalloc(unsigned long size);
void *kalloc_page(void);
void kfree(void *ptr);

#endif
