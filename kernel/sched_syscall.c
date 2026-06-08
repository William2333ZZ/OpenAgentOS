#include "sched.h"
#include "agent.h"

long sched_handle_syscall(long n, long a0, long a1, long a2, long a3) {
    switch (n) {
    case SYS_AGENT_CREATE:
        return agent_create((void (*)(void))a0, (const char *)a1, (unsigned int)a2);
    case SYS_AGENT_DESTROY:
        return agent_destroy((int)a0);
    case SYS_AGENT_SEND:
        return agent_send((int)a0, (int)a1, (const char *)a2);
    case SYS_AGENT_RECV:
        return agent_recv((struct agent_msg *)a0);
    case SYS_AGENT_SELF:
        return agent_current() ? agent_current()->id : -1;
    case SYS_AGENT_WRITE:
        return agent_write((const char *)a0, a1);
    case SYS_AGENT_TOOL:
        return agent_tool((int)a0, a1, a2, a3);
    case SYS_AGENT_LLM:
        return agent_llm_query((const char *)a0, (char *)a1, (int)a2);
    case SYS_AGENT_COMPACT:
        return agent_compact_inbox((int)a0);
    case SYS_AGENT_GET_PHASE:
        return agent_get_phase();
    case SYS_AGENT_SYNC:
        return agent_sync();
    case SYS_AGENT_HEAP_BASE:
        return agent_get_heap_base();
    case SYS_AGENT_HEAP_MAP:
        return agent_heap_map((int)a0);
    case SYS_AGENT_HEAP_COW:
        return agent_heap_cow((int)a0, (int)a1);
    case SYS_AGENT_LOAD:
        return agent_load_from_path((const char *)a0, (const char *)a1, (unsigned int)a2);
    default:
        return EINVAL;
    }
}
