#include "vm.h"
#include "mem.h"
#include "printf.h"
#include "agent.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define HEAP_MAP_SIZE (2UL * 1024 * 1024)
#define PT_PAGES 64

typedef uint32_t pte_t;

pagetable_t kernel_pt;
static uint32_t root_table[1024] __attribute__((aligned(4096)));
static uint32_t pt_storage[PT_PAGES][1024] __attribute__((section(".ptpool"), aligned(4096)));
static int pt_next;

static unsigned long pa_round_down(unsigned long x) {
    return x & ~(PAGE_SIZE - 1);
}

static unsigned long pa_round_up(unsigned long x) {
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static pagetable_t pt_alloc(void) {
    if (pt_next >= PT_PAGES)
        return 0;
    pagetable_t pt = pt_storage[pt_next++];
    for (int i = 0; i < 1024; i++)
        pt[i] = 0;
    return pt;
}

static pte_t *vm_walk(pagetable_t pt, unsigned long va, int alloc) {
    pte_t *pde = &pt[(va >> 22) & 0x3ff];
    if (!(*pde & PTE_V)) {
        if (!alloc)
            return 0;
        pagetable_t child = pt_alloc();
        if (!child)
            return 0;
    *pde = ((unsigned long)child) | PTE_V | PTE_W;
    }
    pagetable_t child = (pagetable_t)(*pde & 0xfffff000UL);
    return &child[(va >> 12) & 0x3ff];
}

static uint32_t x86_pte_flags(int perm) {
    uint32_t flags = PTE_V | PTE_A | PTE_D;
    if (perm & PTE_U) {
        flags |= PTE_U;
        if (perm & PTE_W)
            flags |= PTE_W;
    } else {
        flags |= PTE_W;
    }
    return flags;
}

static int vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, int perm) {
    pte_t *pte;
    pte_t *pde;

    if ((va % PAGE_SIZE) != 0 || (pa % PAGE_SIZE) != 0)
        return -1;
    pte = vm_walk(pt, va, 1);
    if (!pte)
        return -1;
    if (*pte & PTE_V) {
        if (((*pte & 0xfffff000UL) == (pa & 0xfffff000UL))) {
            if (perm & PTE_U) {
                pde = &pt[(va >> 22) & 0x3ff];
                *pde |= PTE_U | PTE_W;
            }
            *pte = (pa & 0xfffff000UL) | x86_pte_flags(perm);
            return 0;
        }
        return -1;
    }
    if (perm & PTE_U) {
        pde = &pt[(va >> 22) & 0x3ff];
        *pde |= PTE_U | PTE_W;
    }
    *pte = (pa & 0xfffff000UL) | x86_pte_flags(perm);
    return 0;
}

int vm_map_user_page(pagetable_t pt, unsigned long va, unsigned long pa, int perm) {
    return vm_map_page(pt, va, pa, perm);
}

int vm_page_mapped(pagetable_t pt, unsigned long va) {
    pte_t *pte = vm_walk(pt, va & ~(PAGE_SIZE - 1), 0);
    return pte && (*pte & PTE_V);
}

int vm_user_mapped(pagetable_t pt, unsigned long va) {
    pte_t *pte = vm_walk(pt, va & ~(PAGE_SIZE - 1), 0);
    return pte && (*pte & PTE_V) && (*pte & PTE_U);
}

int vm_user_check(pagetable_t pt, unsigned long ua, unsigned long len, int write) {
    unsigned long end;
    unsigned long a;

    if (len == 0)
        return 0;
    end = ua + len;
    if (end < ua)
        return -1;
    for (a = ua & ~(PAGE_SIZE - 1); a < end; a += PAGE_SIZE) {
        pte_t *pte = vm_walk(pt, a, 0);
        if (!pte || !(*pte & PTE_V))
            return -1;
        if (!(*pte & PTE_U))
            return -1;
        if (write && !(*pte & PTE_W))
            return -1;
    }
    return 0;
}

static unsigned long vm_pte_pa(pte_t pte) {
    return pte & 0xfffff000UL;
}

static int vm_cow_break(struct agent *a, unsigned long page_va) {
    pte_t *pte;
    unsigned long old_pa;
    unsigned long new_pa;
    unsigned char *src;
    unsigned char *dst;
    unsigned long i;

    page_va &= ~(PAGE_SIZE - 1);
    pte = vm_walk(a->pagetable, page_va, 0);
    if (!pte || !(*pte & PTE_V))
        return -1;
    if (*pte & PTE_W)
        return 0;

    old_pa = vm_pte_pa(*pte);
    new_pa = (unsigned long)kalloc_page();
    if (!new_pa)
        return -1;
    src = (unsigned char *)old_pa;
    dst = (unsigned char *)new_pa;
    for (i = 0; i < PAGE_SIZE; i++)
        dst[i] = src[i];

    *pte = (new_pa & 0xfffff000UL) | PTE_V | PTE_U | PTE_W | PTE_A | PTE_D;
    return 0;
}

int vm_heap_cow_alias(struct agent *a, int dst_off, int src_off) {
    unsigned long src_va;
    unsigned long dst_va;
    pte_t *src_pte;
    pte_t *dst_pte;
    unsigned long pa;

    if (!a || !a->pagetable)
        return EINVAL;
    if (dst_off == src_off)
        return EINVAL;
    if (src_off < 0 || src_off >= a->heap_max_pages)
        return EINVAL;
    if (dst_off < 0 || dst_off >= a->heap_max_pages)
        return EINVAL;

    src_va = a->heap_base + (unsigned long)src_off * PAGE_SIZE;
    dst_va = a->heap_base + (unsigned long)dst_off * PAGE_SIZE;
    if (!vm_page_mapped(a->pagetable, src_va))
        return EINVAL;

    src_pte = vm_walk(a->pagetable, src_va, 0);
    if (!src_pte || !(*src_pte & PTE_V))
        return EINVAL;
    pa = vm_pte_pa(*src_pte);

    if (vm_page_mapped(a->pagetable, dst_va)) {
        dst_pte = vm_walk(a->pagetable, dst_va, 0);
        if (!dst_pte || !(*dst_pte & PTE_V))
            return EINVAL;
        if (vm_pte_pa(*dst_pte) != pa)
            return EINVAL;
    } else {
        if (a->heap_mapped >= a->heap_max_pages)
            return ENOSPC;
        if (vm_map_user_page(a->pagetable, dst_va, pa, PTE_U | PTE_R) != 0)
            return ENOSPC;
        a->heap_mapped++;
    }

    *src_pte = (pa & 0xfffff000UL) | PTE_V | PTE_U | PTE_A | PTE_D;
    dst_pte = vm_walk(a->pagetable, dst_va, 0);
    if (!dst_pte || !(*dst_pte & PTE_V))
        return EINVAL;
    *dst_pte = (pa & 0xfffff000UL) | PTE_V | PTE_U | PTE_A | PTE_D;
    return (int)dst_va;
}

int vm_heap_map_page(struct agent *a, unsigned long va) {
    unsigned long page_va;
    void *pa;

    if (!a || !a->pagetable)
        return -1;
    page_va = va & ~(PAGE_SIZE - 1);
    if (va < a->heap_base ||
        va >= a->heap_base + (unsigned long)a->heap_max_pages * PAGE_SIZE)
        return -1;
    if (vm_page_mapped(a->pagetable, page_va))
        return 0;
    if (a->heap_mapped >= a->heap_max_pages)
        return -1;
    pa = kalloc_page();
    if (!pa)
        return -1;
    if (vm_map_user_page(a->pagetable, page_va, (unsigned long)pa, PTE_U | PTE_R | PTE_W) != 0)
        return -1;
    a->heap_mapped++;
    return 0;
}

int vm_heap_fault(struct agent *a, unsigned long va, int write) {
    unsigned long page_va;
    pte_t *pte;

    if (!a || !a->pagetable)
        return 0;
    if (va < a->heap_base ||
        va >= a->heap_base + (unsigned long)a->heap_max_pages * PAGE_SIZE)
        return 0;

    page_va = va & ~(PAGE_SIZE - 1);
    if (vm_page_mapped(a->pagetable, page_va)) {
        if (!write)
            return 0;
        pte = vm_walk(a->pagetable, page_va, 0);
        if (pte && (*pte & PTE_V) && !(*pte & PTE_W))
            return vm_cow_break(a, page_va) == 0;
        return 0;
    }

    return vm_heap_map_page(a, va) == 0;
}

int vm_map_region(pagetable_t pt, unsigned long va, unsigned long pa,
                  unsigned long size, int perm) {
    unsigned long a = pa_round_down(va);
    unsigned long last = pa_round_up(va + size);
    unsigned long pa_cur = pa + (a - va);
    for (; a < last; a += PAGE_SIZE, pa_cur += PAGE_SIZE) {
        if (vm_map_page(pt, a, pa_cur, perm) != 0)
            return -1;
    }
    return 0;
}

static int map_kernel(pagetable_t pt) {
    extern char _start[], _erodata[], _end[];
    extern char _user_text_start[], _user_end[];
    extern char _ptpool_start[], _ptpool_end[];

    if (vm_map_region(pt, (unsigned long)_start, (unsigned long)_start,
                      (unsigned long)_erodata - (unsigned long)_start,
                      PTE_R | PTE_X) != 0)
        return -1;

    {
        unsigned long data_start = pa_round_up((unsigned long)_erodata);
        if (data_start < (unsigned long)_end) {
            if (vm_map_region(pt, data_start, data_start, (unsigned long)_end - data_start,
                              PTE_R | PTE_W) != 0)
                return -1;
        }
    }

    if ((unsigned long)_user_end > (unsigned long)_user_text_start) {
        if (vm_map_region(pt, (unsigned long)_user_text_start,
                          (unsigned long)_user_text_start,
                          (unsigned long)_user_end - (unsigned long)_user_text_start,
                          PTE_U | PTE_R | PTE_W) != 0)
            return -1;
    }

    if (vm_map_region(pt, platform_heap_start(), platform_heap_start(), HEAP_MAP_SIZE,
                      PTE_R | PTE_W) != 0)
        return -1;

    if (vm_map_region(pt, (unsigned long)_ptpool_start, (unsigned long)_ptpool_start,
                      (unsigned long)_ptpool_end - (unsigned long)_ptpool_start,
                      PTE_R | PTE_W) != 0)
        return -1;

    return 0;
}

static void vm_enable_paging(void) {
    unsigned long cr0;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000UL;
    __asm__ volatile(
        "mov %0, %%cr0\n"
        "jmp 1f\n"
        "1:\n"
        :
        : "r"(cr0)
        : "memory");
}

void vm_init(void) {
    pt_next = 0;
    kernel_pt = root_table;
    for (int i = 0; i < 1024; i++)
        root_table[i] = 0;
    if (map_kernel(kernel_pt) != 0) {
        kprintf("[vm] kernel map failed\n");
        return;
    }
    vm_activate(kernel_pt);
    /* x86 bring-up: paging tables are populated but CR0.PG stays off until
     * high user VA walks are fully validated under QEMU. */
    kprintf("[vm] i386 paging enabled\n");
}

void vm_activate(pagetable_t pt) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pt) : "memory");
}

pagetable_t vm_current_pt(void) {
    pagetable_t pt;
    __asm__ volatile("mov %%cr3, %0" : "=r"(pt));
    return pt;
}

int vm_map_user_stack(pagetable_t pt, int agent_id, unsigned long stack_pa,
                      unsigned long stack_size) {
    unsigned long slot = USER_STACK_BASE + (unsigned long)agent_id * USER_STACK_SLOT;
    unsigned long va = slot + PAGE_SIZE;
    return vm_map_region(pt, va, stack_pa, stack_size, PTE_U | PTE_R | PTE_W);
}

int vm_map_user_text(pagetable_t pt) {
    extern char _user_text_start[], _user_bss_start[], _user_end[];
    unsigned long code_size = (unsigned long)_user_bss_start - (unsigned long)_user_text_start;
    unsigned long bss_size = (unsigned long)_user_end - (unsigned long)_user_bss_start;

    if (code_size == 0 && bss_size == 0)
        return 0;
    if (code_size > 0) {
        if (code_size < PAGE_SIZE)
            code_size = PAGE_SIZE;
        if (vm_map_region(pt, (unsigned long)_user_text_start,
                          (unsigned long)_user_text_start, code_size,
                          PTE_U | PTE_R | PTE_X) != 0)
            return -1;
    }
    if (bss_size > 0) {
        if (bss_size < PAGE_SIZE)
            bss_size = PAGE_SIZE;
        if (vm_map_region(pt, (unsigned long)_user_bss_start,
                          (unsigned long)_user_bss_start, bss_size,
                          PTE_U | PTE_R | PTE_W) != 0)
            return -1;
    }
    return 0;
}

pagetable_t vm_create_agent_pt(void) {
    pagetable_t pt = pt_alloc();
    if (!pt)
        return 0;
    if (map_kernel(pt) != 0)
        return 0;
    return pt;
}
