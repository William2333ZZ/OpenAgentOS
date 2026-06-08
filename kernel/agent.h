#ifndef AGENT_H
#define AGENT_H

#include "../include/agentos.h"
#include "ipc.h"
#include "vm.h"

enum agent_state {
    AGENT_UNUSED = 0,
    AGENT_CREATED,
    AGENT_RUNNABLE,
    AGENT_RUNNING,
    AGENT_WAITING,
    AGENT_ZOMBIE,
};

struct trapframe {
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;
    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    unsigned long s0;
    unsigned long s1;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;
    unsigned long sepc;
    unsigned long sstatus;
};

struct agent {
    int id;
    char name[32];
    enum agent_state state;
    enum agent_phase phase;
    unsigned long stack_base;
    struct trapframe *tf;
    unsigned int caps;
    struct msgbox inbox;
    int exit_code;
    void (*entry)(void);
    pagetable_t pagetable;
    unsigned long heap_base;
    int heap_max_pages;
    int heap_mapped;
    int home_cpu;
    int running_cpu;
};

extern struct agent agents[MAX_AGENTS];
extern int agent_count;

struct agent *agent_current(void);
void agent_set_current(struct agent *a);
#define current_agent agent_current()

void agent_init(void);
int agent_alloc_id(void);
struct agent *agent_get(int id);
int agent_create(void (*entry)(void), const char *name, unsigned int caps);
int agent_send(int dst, int type, const char *payload);
int agent_send_kernel(struct agent *from, int dst, int type, const char *payload);
int agent_send_kernel_nocheck(struct agent *from, int dst, int type,
                              const char *payload);
int agent_recv(struct agent_msg *msg);
int agent_compact_inbox(int keep);
int agent_get_phase(void);
int agent_destroy(int id);
void agent_exit(int code);
int agent_has_cap(struct agent *a, unsigned int cap);
int agent_tool(int tool, long arg0, long arg1, long arg2);
int agent_llm_query(const char *prompt, char *buf, int buflen);
int agent_write(const char *buf, long len);
int agent_start_kernel(struct agent *a, void (*entry)(void));
int agent_start_service(struct agent *a, void (*entry)(void), const char *name,
                        unsigned int caps);
int agent_sync(void);
long agent_get_heap_base(void);
int agent_heap_map(int page_off);
int agent_heap_cow(int dst_off, int src_off);
int agent_load_from_path(const char *path, const char *name, unsigned int caps);
void agent_set_phase(enum agent_phase phase);

#endif
