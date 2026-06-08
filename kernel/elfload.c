#include "elfload.h"
#include "agent.h"
#include "ipc.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "sched.h"
#include "uaccess.h"
#include "vm.h"

#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SUM  (1UL << 18)
#define SSTATUS_SPP  (1UL << 8)

#define ELF_MAGIC 0x464c457fUL
#define PT_LOAD   1
#define PF_X      1
#define PF_W      2
#define PF_R      4

struct elf64_ehdr {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

struct elf64_phdr {
    unsigned int p_type;
    unsigned int p_flags;
    unsigned long p_offset;
    unsigned long p_vaddr;
    unsigned long p_paddr;
    unsigned long p_filesz;
    unsigned long p_memsz;
    unsigned long p_align;
};

static void *memset_local(void *s, int c, unsigned long n) {
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

static void strlcpy_local(char *dst, const char *src, int size) {
    int i;
    for (i = 0; i < size - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int ramfs_file_size(const char *path) {
    char tmp[4];
    int n;

    n = ramfs_read(path, 0, tmp, 1);
    if (n < 0)
        return n;
    for (int off = 1; off < AGENT_ELF_MAX; off++) {
        n = ramfs_read(path, (unsigned long)off, tmp, 1);
        if (n <= 0)
            return off;
    }
    return AGENT_ELF_MAX;
}

static int elf_map_segment(struct agent *a, unsigned long vaddr, unsigned long memsz,
                           unsigned long filesz, const unsigned char *src,
                           int flags) {
    unsigned long va;
    unsigned long end;
    int perm = PTE_U | PTE_R;

    if (flags & PF_W)
        perm |= PTE_W;
    if (flags & PF_X)
        perm |= PTE_X;

    end = vaddr + memsz;
    for (va = vaddr & ~(PAGE_SIZE - 1);
         va < ((end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
         va += PAGE_SIZE) {
        unsigned char *page = kalloc_page();
        unsigned long page_off = (vaddr > va) ? (vaddr - va) : 0;
        unsigned long foff = va + page_off - vaddr;
        unsigned long copy_len = PAGE_SIZE - page_off;
        unsigned long i;

        if (!page)
            return ENOSPC;
        for (i = 0; i < PAGE_SIZE; i++)
            page[i] = 0;
        if (foff < filesz) {
            unsigned long remain = filesz - foff;
            if (copy_len > remain)
                copy_len = remain;
            for (i = 0; i < copy_len; i++)
                page[page_off + i] = src[foff + i];
        }
        if (vm_map_user_page(a->pagetable, va, (unsigned long)page, perm) != 0)
            return ENOSPC;
    }
    return 0;
}

static int elf_load_into_agent(struct agent *a, const unsigned char *elf, int len,
                               unsigned long *entry_out) {
    const struct elf64_ehdr *eh;
    int i;

    if (len < (int)sizeof(struct elf64_ehdr))
        return EINVAL;
    eh = (const struct elf64_ehdr *)elf;
    if (*(unsigned int *)&eh->e_ident[0] != ELF_MAGIC)
        return EINVAL;
    if (eh->e_ident[4] != 2 || eh->e_ident[5] != 1)
        return EINVAL;
    if (eh->e_machine != 243)
        return EINVAL;
    if (eh->e_phentsize != sizeof(struct elf64_phdr))
        return EINVAL;

    for (i = 0; i < eh->e_phnum; i++) {
        const struct elf64_phdr *ph;
        unsigned long off;

        off = eh->e_phoff + (unsigned long)i * eh->e_phentsize;
        if (off + sizeof(struct elf64_phdr) > (unsigned long)len)
            return EINVAL;
        ph = (const struct elf64_phdr *)(elf + off);
        if (ph->p_type != PT_LOAD)
            continue;
        if (ph->p_offset + ph->p_filesz > (unsigned long)len)
            return EINVAL;
        if (elf_map_segment(a, ph->p_vaddr, ph->p_memsz, ph->p_filesz,
                            elf + ph->p_offset, (int)ph->p_flags) != 0)
            return ENOSPC;
    }

    *entry_out = eh->e_entry;
    return 0;
}

static int agent_setup_loaded(struct agent *a, unsigned long entry) {
    unsigned char *stack;
    unsigned long stack_pa;

    (void)entry;
    if (!kalloc_page())
        return ENOSPC;
    stack = kalloc(AGENT_STACK_SIZE);
    if (!stack)
        return ENOSPC;
    stack_pa = ((unsigned long)stack + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    unsigned long stack_va = USER_STACK_BASE + (unsigned long)a->id * USER_STACK_SLOT +
                             PAGE_SIZE;
    unsigned long user_sp = stack_va + AGENT_STACK_SIZE - 256;

    a->pagetable = vm_create_agent_pt();
    if (!a->pagetable)
        return ENOSPC;
    if (vm_map_user_stack(a->pagetable, a->id, stack_pa, AGENT_STACK_SIZE) != 0)
        return ENOSPC;

    struct trapframe *tf = kalloc(sizeof(struct trapframe));
    if (!tf)
        return ENOSPC;
    unsigned long gp;
    __asm__ volatile("mv %0, gp" : "=r"(gp));
    memset_local(tf, 0, sizeof(*tf));
    tf->sepc = 0;
    tf->sp = user_sp;
    tf->ra = 0;
    tf->gp = gp;
    tf->sstatus = SSTATUS_SPIE | SSTATUS_SUM;
    tf->sstatus &= ~SSTATUS_SPP;
    a->stack_base = stack_va + AGENT_STACK_SIZE;
    a->tf = tf;
    a->entry = 0;
    a->heap_base = USER_HEAP_BASE + (unsigned long)a->id * AGENT_HEAP_SIZE;
    a->heap_max_pages = (int)(AGENT_HEAP_SIZE / PAGE_SIZE);
    a->heap_mapped = 0;
    return 0;
}

int agent_load_from_path(const char *path, const char *name, unsigned int caps) {
    char kpath[64];
    char kname[sizeof(((struct agent *)0)->name)];
    unsigned char *elf;
    int len;
    int id;
    struct agent *a;
    unsigned long entry;
    int nlen;
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_SPAWN))
        return EPERM;
    if (!path || !name)
        return EINVAL;

    len = user_strnlen(path, (int)sizeof(kpath) - 1);
    if (len <= 0)
        return EFAULT;
    if (copy_from_user(kpath, path, (unsigned long)len + 1) < 0)
        return EFAULT;

    len = ramfs_file_size(kpath);
    if (len < 0)
        return len;
    if (len < 64 || len > AGENT_ELF_MAX)
        return EINVAL;

    elf = kalloc((unsigned long)len);
    if (!elf)
        return ENOSPC;
    rc = ramfs_read(kpath, 0, (char *)elf, len);
    if (rc != len)
        return (rc < 0) ? rc : EIO;

    id = agent_alloc_id();
    if (id < 0)
        return ENOSPC;

    a = &agents[id];
    a->id = id;
    rc = agent_setup_loaded(a, 0);
    if (rc != 0) {
        kfree(elf);
        return rc;
    }

    rc = elf_load_into_agent(a, elf, len, &entry);
    kfree(elf);
    if (rc != 0)
        return rc;
    a->tf->sepc = entry;
    a->entry = (void (*)(void))entry;

    nlen = user_strnlen(name, (int)sizeof(kname) - 1);
    if (nlen < 0)
        return EFAULT;
    if (nlen == 0)
        return EINVAL;
    if (copy_from_user(kname, name, (unsigned long)nlen + 1) < 0)
        return EFAULT;
    strlcpy_local(a->name, kname, sizeof(a->name));
    a->caps = caps;
    a->state = AGENT_RUNNABLE;
    a->phase = AGENT_PHASE_IDLE;
    a->exit_code = 0;
    msgbox_init(&a->inbox);
    agent_count++;

    kprintf("[kernel] agent_load id=%d name=%s entry=%lx caps=%u size=%d\n",
            id, a->name, entry, caps, len);
    sched_notify_runnable(a);
    return id;
}
