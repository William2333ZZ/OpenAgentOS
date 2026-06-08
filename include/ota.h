#ifndef OTA_H
#define OTA_H

#include <stdint.h>

#define OTA_PKG_MAGIC   0x4b504f41u /* 'AOPK' little-endian */
#define OTA_FMT_VERSION 1

#define OTA_CHANNEL_STABLE "stable"
#define OTA_CHANNEL_BETA   "beta"

#define OTA_PATH_CHANNEL   "/sys/ota/channel"
#define OTA_PATH_STAGED    "/agent/1/worker-v2.agentpkg"

struct ota_manifest {
    uint32_t magic;
    uint32_t format;
    uint32_t sig;
    char name[32];
    char version[16];
    char channel[8];
    uint32_t caps;
    uint32_t elf_offset;
    uint32_t elf_size;
    char install_path[48];
};

#define OTA_MANIFEST_SIZE 128

void ota_init(void);
int ota_kernel_gen(void);
int ota_channel_set(const char *channel);
int ota_channel_get(char *buf, int buflen);
int ota_verify_package(const char *path);
int ota_apply_package(const char *path);
int ota_verify_package_kpath(const char *path);
int ota_apply_package_kpath(const char *path);

#endif
