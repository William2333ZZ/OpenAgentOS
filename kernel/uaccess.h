#ifndef UACCESS_H
#define UACCESS_H

long copy_from_user(void *dst, const void *user_src, unsigned long len);
long copy_to_user(void *user_dst, const void *src, unsigned long len);
int user_strnlen(const char *user_s, int max);
int user_access_ok(const void *user_ptr, unsigned long len, int write);

#endif
