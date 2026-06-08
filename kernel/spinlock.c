#include "spinlock.h"

void spinlock_init(spinlock_t *lk) {
    lk->locked = 0;
}

void spinlock_acquire(spinlock_t *lk) {
    while (__sync_lock_test_and_set(&lk->locked, 1))
        ;
}

void spinlock_release(spinlock_t *lk) {
    __sync_lock_release(&lk->locked);
}
