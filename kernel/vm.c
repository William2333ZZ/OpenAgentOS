#include "vm.h"
#include "mem.h"
#include "printf.h"
#include "agent.h"
#include "../include/agentos.h"
#include "../include/platform.h"

#define SATP_SV39 (8UL << 60)
#define HEAP_MAP_SIZE (2UL * 1024 * 1024)
#define PT_PAGES 64

typedef uint64_t pte_t;

pagetable_t kernel_pt;
static uint64_t root_table[512] __attribute__((aligned(4096)));
static uint64_t pt_storage[PT_PAGES][512] __attribute__((section(".ptpool"), aligned(4096)));
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
    for (int i = 0; i < 512; i++)
        pt[i] = 0;
    return pt;
}

static pte_t *vm_walk(pagetable_t pt, unsigned long va, int alloc) {
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[((va >> (12 + 9 * level)) & 0x1ff)];
        if (*pte & PTE_V) {
            pt = (pagetable_t)(((*pte) >> 10) << PAGE_SHIFT);
        } else {
            if (!alloc)
                return 0;
            pagetable_t child = pt_alloc();
            if (!child)
                return 0;
            *pte = (((unsigned long)child) >> PAGE_SHIFT << 10) | PTE_V;
            pt = child;
        }
    }
    return &pt[(va >> 12) & 0x1ff];
}

static int vm_map_page(pagetable_t pt, unsigned long va, unsigned long pa, int perm) {
    if ((va % PAGE_SIZE) != 0 || (pa % PAGE_SIZE) != 0)
        return -1;
    pte_t *pte = vm_walk(pt, va, 1);
    if (!pte)
        return -1;
    if (*pte & PTE_V)
        return -1;
    *pte = ((pa >> PAGE_SHIFT) << 10) | perm | PTE_V | PTE_STD;
    return 0;
}

int vm_map_user_page(pagetable_t pt, unsigned long va, unsigned long pa, int perm) {
    return vm_map_page(pt, va, pa, perm);
}

int vm_page_mapped(pagetable_t pt, unsigned long va) {
    pte_t *pte = vm_walk(pt, va & ~(PAGE_SIZE - 1), 0);
    return pte && (*pte & PTE_V);
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
        if (write) {
            if (!(*pte & PTE_W))
                return -1;
        } else if (!(*pte & PTE_R) && !(*pte & PTE_X)) {
            return -1;
        }
    }
    return 0;
}

static unsigned long vm_pte_pa(pte_t pte) {
    return (pte >> 10) << PAGE_SHIFT;
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

    *pte = ((new_pa >> PAGE_SHIFT) << 10) | PTE_U | PTE_R | PTE_W | PTE_V | PTE_STD;
    kprintf("[vm] heap cow break agent=%d va=%lx pa=%lx\n", a->id, page_va, new_pa);
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

    *src_pte = ((pa >> PAGE_SHIFT) << 10) | PTE_U | PTE_R | PTE_V | PTE_STD;
    dst_pte = vm_walk(a->pagetable, dst_va, 0);
    if (!dst_pte || !(*dst_pte & PTE_V))
        return EINVAL;
    *dst_pte = ((pa >> PAGE_SHIFT) << 10) | PTE_U | PTE_R | PTE_V | PTE_STD;

    kprintf("[vm] heap cow alias agent=%d src=%d dst=%d pa=%lx\n",
            a->id, src_off, dst_off, pa);
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
    if (vm_map_user_page(a->pagetable, page_va, (unsigned long)pa,
                         PTE_U | PTE_R | PTE_W) != 0)
        return -1;
    a->heap_mapped++;
    kprintf("[vm] heap map agent=%d va=%lx page %d/%d\n", a->id, page_va,
            a->heap_mapped, a->heap_max_pages);
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

#define ACLINT_MSWI_BASE 0x02000000UL

static int map_mmio(pagetable_t pt) {
    unsigned long uart = platform_uart_mmio();
    unsigned long vio = platform_virtio_mmio_base();
    int count = platform_virtio_mmio_count();
    unsigned long vio_size = (unsigned long)count * platform_virtio_mmio_stride();

    if (vm_map_region(pt, uart, uart, 0x00001000UL, PTE_R | PTE_W) != 0)
        return -1;
    if (vm_map_region(pt, ACLINT_MSWI_BASE, ACLINT_MSWI_BASE, 0x00008000UL,
                      PTE_R | PTE_W) != 0)
        return -1;
    if (count > 0 && vio_size > 0) {
        if (vm_map_region(pt, vio, vio, vio_size, PTE_R | PTE_W) != 0)
            return -1;
    }
    return 0;
}

static int map_kernel(pagetable_t pt) {
    extern char _start[], _erodata[], _end[];
    extern char _ptpool_start[], _ptpool_end[];

    if (vm_map_region(pt, (unsigned long)_start, (unsigned long)_start,
                      (unsigned long)_erodata - (unsigned long)_start,
                      PTE_R | PTE_X) != 0)
        return -1;

    unsigned long data_start = pa_round_up((unsigned long)_erodata);
    if (data_start < (unsigned long)_end) {
        if (vm_map_region(pt, data_start, data_start, (unsigned long)_end - data_start,
                          PTE_R | PTE_W) != 0)
            return -1;
    }

    if (vm_map_region(pt, platform_heap_start(), platform_heap_start(), HEAP_MAP_SIZE,
                      PTE_R | PTE_W) != 0)
        return -1;

    if (vm_map_region(pt, (unsigned long)_ptpool_start, (unsigned long)_ptpool_start,
                      (unsigned long)_ptpool_end - (unsigned long)_ptpool_start,
                      PTE_R | PTE_W) != 0)
        return -1;

    return map_mmio(pt);
}

void vm_init(void) {
    pt_next = 0;
    kernel_pt = root_table;
    for (int i = 0; i < 512; i++)
        root_table[i] = 0;
    if (map_kernel(kernel_pt) != 0) {
        kprintf("[vm] kernel map failed\n");
        return;
    }
    if (vm_map_user_text(kernel_pt) != 0) {
        kprintf("[vm] user text map failed\n");
        return;
    }
    vm_activate(kernel_pt);
    kprintf("[vm] SV39 enabled\n");
}

void vm_activate(pagetable_t pt) {
    unsigned long satp = SATP_SV39 | (((unsigned long)pt) >> PAGE_SHIFT);
    __asm__ volatile("csrw satp, %0" : : "r"(satp));
    __asm__ volatile("sfence.vma x0, x0" : : : "memory");
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
    int i;

    if (!pt || !kernel_pt)
        return 0;
    for (i = 0; i < 512; i++)
        pt[i] = kernel_pt[i];
    return pt;
}
