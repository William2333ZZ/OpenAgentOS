#include "ipc.h"
#include "spinlock.h"

static spinlock_t msgbox_lock;

static void msgbox_lock_init_once(void) {
    static int inited;
    if (!inited) {
        spinlock_init(&msgbox_lock);
        inited = 1;
    }
}

static void msg_copy(struct agent_msg *dst, const struct agent_msg *src) {
    dst->sender_id = src->sender_id;
    dst->type = src->type;
    for (int i = 0; i < MSG_PAYLOAD_SIZE; i++)
        dst->payload[i] = src->payload[i];
}

static void payload_putc(char *dst, int cap, int *pos, char c) {
    if (*pos < cap - 1)
        dst[(*pos)++] = c;
}

static void payload_puts(char *dst, int cap, int *pos, const char *s) {
    while (*s && *pos < cap - 1)
        dst[(*pos)++] = *s++;
}

static void payload_put_uint(char *dst, int cap, int *pos, unsigned int n) {
    char tmp[12];
    int i = 0;
    if (n == 0) {
        payload_putc(dst, cap, pos, '0');
        return;
    }
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        payload_putc(dst, cap, pos, tmp[--i]);
}

void msgbox_init(struct msgbox *box) {
    box->normal_head = box->normal_tail = box->normal_count = 0;
    box->steer_head = box->steer_tail = box->steer_count = 0;
}

int msgbox_is_priority_type(int type) {
    return type == MSG_STEER;
}

int msgbox_empty(const struct msgbox *box) {
    return box->normal_count == 0 && box->steer_count == 0;
}

int msgbox_push(struct msgbox *box, const struct agent_msg *msg) {
    int rc;

    msgbox_lock_init_once();
    spinlock_acquire(&msgbox_lock);
    if (msgbox_is_priority_type(msg->type)) {
        if (box->steer_count >= MSG_STEER_CAPACITY) {
            rc = ENOSPC;
            goto out;
        }
        msg_copy(&box->steer[box->steer_tail], msg);
        box->steer_tail = (box->steer_tail + 1) % MSG_STEER_CAPACITY;
        box->steer_count++;
        rc = 0;
        goto out;
    }

    if (box->normal_count >= MSG_CAPACITY) {
        rc = ENOSPC;
        goto out;
    }
    msg_copy(&box->normal[box->normal_tail], msg);
    box->normal_tail = (box->normal_tail + 1) % MSG_CAPACITY;
    box->normal_count++;
    rc = 0;
out:
    spinlock_release(&msgbox_lock);
    return rc;
}

int msgbox_pop(struct msgbox *box, struct agent_msg *msg) {
    int rc;

    msgbox_lock_init_once();
    spinlock_acquire(&msgbox_lock);
    if (box->steer_count > 0) {
        msg_copy(msg, &box->steer[box->steer_head]);
        box->steer_head = (box->steer_head + 1) % MSG_STEER_CAPACITY;
        box->steer_count--;
        rc = 0;
        goto out;
    }
    if (box->normal_count == 0) {
        rc = EAGAIN;
        goto out;
    }
    msg_copy(msg, &box->normal[box->normal_head]);
    box->normal_head = (box->normal_head + 1) % MSG_CAPACITY;
    box->normal_count--;
    rc = 0;
out:
    spinlock_release(&msgbox_lock);
    return rc;
}

int msgbox_compact(struct msgbox *box, int keep) {
    struct agent_msg ordered[MSG_CAPACITY];
    struct agent_msg rebuilt[MSG_CAPACITY + 1];
    int total;
    int drop;
    int summary_pos = 0;
    int out = 0;
    int i;
    int rc;

    msgbox_lock_init_once();
    spinlock_acquire(&msgbox_lock);

    total = box->normal_count;
    if (keep < 0)
        keep = 0;
    if (keep > MSG_CAPACITY)
        keep = MSG_CAPACITY;
    if (total <= keep) {
        rc = 0;
        goto out;
    }

    drop = total - keep;
    for (i = 0; i < total; i++) {
        int idx = (box->normal_head + i) % MSG_CAPACITY;
        msg_copy(&ordered[i], &box->normal[idx]);
    }

    rebuilt[0].sender_id = 0;
    rebuilt[0].type = MSG_SUMMARY;
    for (i = 0; i < MSG_PAYLOAD_SIZE; i++)
        rebuilt[0].payload[i] = 0;
    payload_puts(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos, "compact:");
    payload_put_uint(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos, (unsigned int)drop);
    payload_puts(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos, ";");
    for (i = 0; i < drop && summary_pos < MSG_PAYLOAD_SIZE - 8; i++) {
        if (i > 0)
            payload_putc(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos, ',');
        payload_putc(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos, 't');
        payload_put_uint(rebuilt[0].payload, MSG_PAYLOAD_SIZE, &summary_pos,
                         (unsigned int)ordered[i].type);
    }
    out = 1;
    for (i = drop; i < total; i++)
        msg_copy(&rebuilt[out++], &ordered[i]);

    box->normal_head = 0;
    box->normal_tail = out % MSG_CAPACITY;
    box->normal_count = out;
    for (i = 0; i < out; i++)
        msg_copy(&box->normal[i], &rebuilt[i]);
    rc = 0;
out:
    spinlock_release(&msgbox_lock);
    return rc;
}
