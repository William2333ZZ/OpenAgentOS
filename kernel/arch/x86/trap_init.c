#include "trap.h"
#include "timer.h"

#define IDT_ENTRIES 256
#define TSS_SEG     0x28

struct gdt_entry {
    unsigned short limit_lo;
    unsigned short base_lo;
    unsigned char base_mid;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_hi;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

struct idt_entry {
    unsigned short offset_lo;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_hi;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

struct tss_entry {
    unsigned int prev_tss;
    unsigned int esp0;
    unsigned int ss0;
    unsigned int esp1;
    unsigned int ss1;
    unsigned int esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap;
} __attribute__((packed));

extern void trap_entry_gpf(void);
extern void trap_entry_page(void);
extern void trap_entry_syscall(void);
extern void trap_entry_timer(void);
extern char stack_top[];

static struct gdt_entry gdt[7];
static struct idt_entry idt[IDT_ENTRIES];
static struct tss_entry cpu_tss;

static void gdt_set(int idx, unsigned long base, unsigned long limit, unsigned char access,
                    unsigned char gran) {
    gdt[idx].limit_lo = (unsigned short)(limit & 0xffff);
    gdt[idx].base_lo = (unsigned short)(base & 0xffff);
    gdt[idx].base_mid = (unsigned char)((base >> 16) & 0xff);
    gdt[idx].access = access;
    gdt[idx].granularity = gran ? gran : (unsigned char)((limit >> 16) & 0x0f);
    gdt[idx].base_hi = (unsigned char)((base >> 24) & 0xff);
}

static void gdt_set_tss(int idx, unsigned long base, unsigned long limit) {
    gdt_set(idx, base, limit, 0x89, 0x00);
}

static void idt_set(int vec, void (*handler)(void), unsigned char type_attr) {
    unsigned long addr = (unsigned long)handler;
    idt[vec].offset_lo = (unsigned short)(addr & 0xffff);
    idt[vec].selector = 0x08;
    idt[vec].zero = 0;
    idt[vec].type_attr = type_attr;
    idt[vec].offset_hi = (unsigned short)((addr >> 16) & 0xffff);
}

void trap_init(void) {
    struct gdt_ptr gp;
    struct idt_ptr ip;

    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xffffffffUL, 0x9a, 0xcf);
    gdt_set(2, 0, 0xffffffffUL, 0x92, 0xcf);
    gdt_set(3, 0, 0xffffffffUL, 0xfa, 0xcf);
    gdt_set(4, 0, 0xffffffffUL, 0xf2, 0xcf);

    cpu_tss.ss0 = 0x10;
    cpu_tss.esp0 = (unsigned int)(unsigned long)&stack_top;
    cpu_tss.iomap = sizeof(cpu_tss);
    gdt_set_tss(5, (unsigned long)&cpu_tss, sizeof(cpu_tss) - 1);

    gp.limit = (unsigned short)(sizeof(gdt) - 1);
    gp.base = (unsigned long)gdt;
    __asm__ volatile("lgdt %0" : : "m"(gp));

    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        :
        :
        : "eax");

    __asm__ volatile("ltr %0" : : "r"((unsigned short)TSS_SEG));

    idt_set(0x0d, trap_entry_gpf, 0x8e);
    idt_set(0x0e, trap_entry_page, 0x8e);
    idt_set(0x20, trap_entry_timer, 0x8e);
    idt_set(0x80, trap_entry_syscall, 0xee);

    ip.limit = (unsigned short)(sizeof(idt) - 1);
    ip.base = (unsigned long)idt;
    __asm__ volatile("lidt %0" : : "m"(ip));

    timer_init();
}
