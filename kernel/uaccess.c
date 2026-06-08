#include "uaccess.h"
#include "agent.h"
#include "vm.h"
#include "../include/agentos.h"

int user_access_ok(const void *user_ptr, unsigned long len, int write) {
    unsigned long ua;

    if (!current_agent || !current_agent->pagetable || !user_ptr)
        return 0;
    ua = (unsigned long)user_ptr;
    return vm_user_check(current_agent->pagetable, ua, len, write) == 0;
}

long copy_from_user(void *dst, const void *user_src, unsigned long len) {
    unsigned char *d = dst;
    const unsigned char *s = user_src;
    unsigned long i;

    if (!dst || !user_src)
        return EFAULT;
    if (len == 0)
        return 0;
    if (!user_access_ok(user_src, len, 0))
        return EFAULT;
    for (i = 0; i < len; i++)
        d[i] = s[i];
    return (long)len;
}

long copy_to_user(void *user_dst, const void *src, unsigned long len) {
    unsigned char *d = user_dst;
    const unsigned char *s = src;
    unsigned long i;

    if (!user_dst || !src)
        return EFAULT;
    if (len == 0)
        return 0;
    if (!user_access_ok(user_dst, len, 1))
        return EFAULT;
    for (i = 0; i < len; i++)
        d[i] = s[i];
    return (long)len;
}

int user_strnlen(const char *user_s, int max) {
    char c;
    int n;

    if (!user_s || max <= 0)
        return EFAULT;
    for (n = 0; n < max; n++) {
        if (!user_access_ok(user_s + n, 1, 0))
            return EFAULT;
        if (copy_from_user(&c, user_s + n, 1) != 1)
            return EFAULT;
        if (c == '\0')
            return n;
    }
    return n;
}
