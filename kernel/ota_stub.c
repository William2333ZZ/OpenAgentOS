#include "ota.h"
#include "../include/agentos.h"

void ota_init(void) {}

int ota_kernel_gen(void) {
    return 1;
}

int ota_channel_set(const char *channel) {
    (void)channel;
    return ENODEV;
}

int ota_channel_get(char *buf, int buflen) {
    (void)buf;
    (void)buflen;
    return ENODEV;
}

int ota_verify_package(const char *path) {
    (void)path;
    return ENODEV;
}

int ota_apply_package(const char *path) {
    (void)path;
    return ENODEV;
}

int ota_verify_package_kpath(const char *path) {
    (void)path;
    return ENODEV;
}

int ota_apply_package_kpath(const char *path) {
    (void)path;
    return ENODEV;
}
