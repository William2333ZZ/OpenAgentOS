#ifndef IPC_H
#define IPC_H

#include "../include/agentos.h"

struct msgbox {
    struct agent_msg normal[MSG_CAPACITY];
    int normal_head;
    int normal_tail;
    int normal_count;
    struct agent_msg steer[MSG_STEER_CAPACITY];
    int steer_head;
    int steer_tail;
    int steer_count;
};

void msgbox_init(struct msgbox *box);
int msgbox_is_priority_type(int type);
int msgbox_push(struct msgbox *box, const struct agent_msg *msg);
int msgbox_pop(struct msgbox *box, struct agent_msg *msg);
int msgbox_empty(const struct msgbox *box);
int msgbox_compact(struct msgbox *box, int keep);

#endif
