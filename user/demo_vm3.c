#include "../user/libagent.h"

static void stack_guard_worker(void) {
    unsigned long guard_va = USER_STACK_BASE +
                             (unsigned long)agent_self() * USER_STACK_SLOT;

    agent_log_str("[stack] touch guard page below stack\n");
    *(volatile char *)guard_va = 1;
    agent_log_str("[stack] should not reach here\n");
    agent_exit(0);
}

static void bad_worker(void) {
    volatile int *p = (volatile int *)(unsigned long)0xdeadbeefUL;

    agent_log_str("[bad] touching invalid ptr 0xdeadbeef\n");
    *p = 42;
    agent_log_str("[bad] should not reach here\n");
    agent_exit(0);
}

static void good_worker(void) {
    char *heap = (char *)agent_heap_base();
    long mapped;

    if (!heap) {
        agent_log_str("[good] heap base null\n");
        agent_exit(1);
    }

    agent_log_str("[good] heap base ready, demand map test\n");

    heap[0] = 'H';
    heap[1] = 'i';
    heap[2] = '!';
    heap[3] = '\0';

    agent_yield();

    if (heap[0] != 'H' || heap[1] != 'i') {
        agent_log_str("[good] heap readback failed\n");
        agent_exit(1);
    }

    mapped = sys_agent_heap_map(1);
    if (mapped < 0) {
        agent_log_str("[good] heap_map page1 failed\n");
        agent_exit(1);
    }

    ((char *)mapped)[0] = 'X';
    agent_log_str("[good] heap demand map ok\n");
    agent_log_str("[good] heap=");
    agent_log_str(heap);
    agent_log_str("\n");

    if (agent_heap_cow_page(2, 0) < 0) {
        agent_log_str("[good] heap_cow page2<-0 failed\n");
        agent_exit(1);
    }
    {
        char *alias = heap + 4096 + 4096;
        if (alias[0] != 'H') {
            agent_log_str("[good] cow alias read failed\n");
            agent_exit(1);
        }
        alias[0] = 'Q';
        if (heap[0] != 'H') {
            agent_log_str("[good] cow src mutated\n");
            agent_exit(1);
        }
        if (alias[0] != 'Q') {
            agent_log_str("[good] cow alias write failed\n");
            agent_exit(1);
        }
    }
    agent_log_str("[good] heap COW ok\n");

    agent_send_msg(1, MSG_RESULT, "heap-ok");
    agent_exit(0);
}

static void probe_worker(void) {
    long rc;

    agent_log_str("[probe] bad pointer via agent_write\n");
    rc = sys_agent_write((const char *)(unsigned long)0xdead0000UL, 5);
    if (rc != EFAULT) {
        agent_log_str("[probe] expected EFAULT for bad buf ptr\n");
        agent_exit(1);
    }
    agent_log_str("[probe] syscall EFAULT ok\n");
    agent_send_msg(1, MSG_RESULT, "probe-ok");
    agent_exit(0);
}

void init_vm3_agent(void) {
    struct agent_msg msg;
    int bad_id;
    int stack_id;
    int good_id;
    int probe_id;

    agent_log_str("[init] v3.0 VM: demand heap + user probe + fault isolation\n");

    probe_id = agent_spawn(probe_worker, "probe", CAP_LOG | CAP_SEND);
    agent_log_str("[init] spawned probe id=");
    agent_log_int(probe_id);
    agent_log_str("\n");
    agent_recv_msg(&msg);

    bad_id = agent_spawn(bad_worker, "bad", CAP_LOG);
    stack_id = agent_spawn(stack_guard_worker, "stack-guard", CAP_LOG);
    good_id = agent_spawn(good_worker, "good", CAP_LOG | CAP_SEND);
    agent_log_str("[init] spawned bad=");
    agent_log_int(bad_id);
    agent_log_str(" stack-guard=");
    agent_log_int(stack_id);
    agent_log_str(" good=");
    agent_log_int(good_id);
    agent_log_str("\n");

    agent_recv_msg(&msg);
    if (msg.type != MSG_RESULT || msg.payload[0] != 'h') {
        agent_log_str("[init] good worker failed\n");
        agent_exit(1);
    }
    agent_log_str("[init] good worker: ");
    agent_log_str(msg.payload);
    agent_log_str("\n");
    agent_log_str("[init] bad/stack agents faulted (see kernel log)\n");
    agent_log_str("[init] vm3 demo complete\n");
    agent_exit(0);
}
