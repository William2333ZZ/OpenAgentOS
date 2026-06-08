#ifndef VM_H
#define VM_H

#include "../include/agentos.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE  (1UL << PAGE_SHIFT)
#define PTE_V 0x001UL
#define PTE_R 0x002UL
#define PTE_W 0x004UL
#define PTE_X 0x008UL
#define PTE_U 0x010UL
#define PTE_A 0x040UL
#define PTE_D 0x080UL
#define PTE_STD (PTE_A | PTE_D)

#define AGENT_HEAP_SIZE (16UL * PAGE_SIZE)

struct agent;

#ifdef PLATFORM_X86_64_PC
typedef uint32_t *pagetable_t;
#else
typedef uint64_t *pagetable_t;
#endif

void vm_init(void);
pagetable_t vm_current_pt(void);
void vm_activate(pagetable_t pt);
int vm_map_region(pagetable_t pt, unsigned long va, unsigned long pa,
                  unsigned long size, int perm);
int vm_map_user_stack(pagetable_t pt, int agent_id, unsigned long stack_pa,
                      unsigned long stack_size);
int vm_map_user_text(pagetable_t pt);
pagetable_t vm_create_agent_pt(void);
int vm_user_check(pagetable_t pt, unsigned long ua, unsigned long len, int write);
int vm_page_mapped(pagetable_t pt, unsigned long va);
int vm_user_mapped(pagetable_t pt, unsigned long va);
int vm_map_user_page(pagetable_t pt, unsigned long va, unsigned long pa, int perm);
int vm_heap_map_page(struct agent *a, unsigned long va);
int vm_heap_cow_alias(struct agent *a, int dst_off, int src_off);
int vm_heap_fault(struct agent *a, unsigned long va, int write);

extern pagetable_t kernel_pt;

#endif
