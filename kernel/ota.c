#include "ota.h"
#include "agent.h"
#include "mem.h"
#include "printf.h"
#include "ramfs.h"
#include "uaccess.h"
#include "../include/agentos.h"

static uint32_t ota_crc32_update(uint32_t crc, const unsigned char *data,
                                 unsigned long len) {
    unsigned long i;
    int bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xedb88320u;
            else
                crc >>= 1;
        }
    }
    return crc;
}

static void *memcpy_local(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

static int ota_file_size(const char *path) {
    char tmp[4];
    int n;

    n = ramfs_read(path, 0, tmp, 1);
    if (n < 0)
        return n;
    for (int off = 1; off < RAMFS_FILE_SIZE; off++) {
        n = ramfs_read(path, (unsigned long)off, tmp, 1);
        if (n <= 0)
            return off;
    }
    return RAMFS_FILE_SIZE;
}

static int ota_pkg_path_ok(const char *path) {
    int id = 0;
    int i;

    if (path[1] == 's' && path[2] == 'y' && path[3] == 's' && path[4] == '/' &&
        path[5] == 'o' && path[6] == 't' && path[7] == 'a' && path[8] == '/')
        return 1;
    if (path[1] == 'a' && path[2] == 'g' && path[3] == 'e' && path[4] == 'n' &&
        path[5] == 't' && path[6] == '/') {
        i = 7;
        while (path[i] >= '0' && path[i] <= '9') {
            id = id * 10 + (path[i] - '0');
            i++;
        }
        if (path[i] == '/' && current_agent && id == current_agent->id)
            return 1;
    }
    return 0;
}

static int ota_channel_matches(const char *pkg_channel) {
    char cur[16];
    int n;

    n = ramfs_read(OTA_PATH_CHANNEL, 0, cur, (int)sizeof(cur) - 1);
    if (n <= 0) {
        if (pkg_channel[0] == 's')
            return 1;
        return 0;
    }
    cur[n] = '\0';
    for (int i = 0; i < 8; i++) {
        char a = cur[i];
        char b = pkg_channel[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));
        if (a != b)
            return 0;
        if (a == '\0')
            return 1;
    }
    return 1;
}

static uint32_t ota_package_sig(const unsigned char *pkg, uint32_t elf_offset,
                                  uint32_t total) {
    struct ota_manifest tmp;
    uint32_t crc;

    memcpy_local(&tmp, pkg, sizeof(tmp));
    tmp.sig = 0;
    crc = 0xffffffffu;
    crc = ota_crc32_update(crc, (const unsigned char *)&tmp, sizeof(tmp));
    if (elf_offset > OTA_MANIFEST_SIZE)
        crc = ota_crc32_update(crc, pkg + OTA_MANIFEST_SIZE,
                               elf_offset - OTA_MANIFEST_SIZE);
    if (total > elf_offset)
        crc = ota_crc32_update(crc, pkg + elf_offset, total - elf_offset);
    return crc ^ 0xffffffffu;
}

static int ota_parse_package(const char *path, struct ota_manifest *manifest_out,
                             const unsigned char **elf_out, int *elf_len_out) {
    unsigned char *buf;
    int len;
    const struct ota_manifest *m;
    int rc;

    len = ota_file_size(path);
    if (len < 0)
        return len;
    if (len < (int)OTA_MANIFEST_SIZE + 64)
        return EINVAL;

    buf = kalloc((unsigned long)len);
    if (!buf)
        return ENOSPC;

    rc = ramfs_read(path, 0, (char *)buf, len);
    if (rc != len)
        return (rc < 0) ? rc : EIO;

    m = (const struct ota_manifest *)buf;
    if (m->magic != OTA_PKG_MAGIC || m->format != OTA_FMT_VERSION)
        return EINVAL;
    if (m->elf_offset < OTA_MANIFEST_SIZE)
        return EINVAL;
    if (m->elf_offset + m->elf_size > (uint32_t)len)
        return EINVAL;
    if (m->install_path[0] != '/')
        return EINVAL;
    if (ota_package_sig(buf, m->elf_offset, (uint32_t)len) != m->sig)
        return EPERM;
    if (!ota_channel_matches(m->channel))
        return EPERM;

    memcpy_local(manifest_out, m, sizeof(*manifest_out));
    *elf_out = buf + m->elf_offset;
    *elf_len_out = (int)m->elf_size;
    return 0;
}

void ota_init(void) {
    char cur[16];
    int n;

    n = ramfs_read(OTA_PATH_CHANNEL, 0, cur, (int)sizeof(cur) - 1);
    if (n > 0)
        return;
    ramfs_put(OTA_PATH_CHANNEL, OTA_CHANNEL_STABLE, 6);
    kprintf("[ota] channel default=%s\n", OTA_CHANNEL_STABLE);
}

int ota_kernel_gen(void) {
    return 2;
}

int ota_channel_set(const char *channel) {
    char kchan[16];
    int len;

    if (!channel)
        return EINVAL;
    len = user_strnlen(channel, (int)sizeof(kchan) - 1);
    if (len <= 0)
        return EINVAL;
    if (copy_from_user(kchan, channel, (unsigned long)len + 1) < 0)
        return EFAULT;
    if (ramfs_put(OTA_PATH_CHANNEL, kchan, len) != 0)
        return ENOSPC;
    kprintf("[ota] channel set to %s\n", kchan);
    return 0;
}

int ota_channel_get(char *buf, int buflen) {
    int n;

    if (!buf || buflen <= 0)
        return EINVAL;
    n = ramfs_read(OTA_PATH_CHANNEL, 0, buf, buflen - 1);
    if (n < 0)
        return n;
    buf[n] = '\0';
    return n;
}

int ota_verify_package_kpath(const char *kpath) {
    struct ota_manifest manifest;
    const unsigned char *elf;
    int elf_len;
    int rc;

    if (!kpath)
        return EINVAL;

    rc = ota_parse_package(kpath, &manifest, &elf, &elf_len);
    if (rc != 0)
        return rc;

    kprintf("[ota] verify ok pkg=%s name=%s ver=%s channel=%s size=%d\n",
            kpath, manifest.name, manifest.version, manifest.channel, elf_len);
    return 0;
}

int ota_apply_package_kpath(const char *kpath) {
    struct ota_manifest manifest;
    const unsigned char *elf;
    int elf_len;
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_FS))
        return EPERM;
    if (!kpath)
        return EINVAL;
    if (!ota_pkg_path_ok(kpath))
        return EPERM;

    rc = ota_parse_package(kpath, &manifest, &elf, &elf_len);
    if (rc != 0)
        return rc;

    if (ramfs_put(manifest.install_path, (const char *)elf, elf_len) != 0)
        return ENOSPC;

    kprintf("[ota] applied %s -> %s (%s %s %d bytes)\n",
            kpath, manifest.install_path, manifest.name, manifest.version, elf_len);
    return 0;
}

int ota_verify_package(const char *path) {
    char kpath[64];
    int len;
    int rc;

    if (!path)
        return EINVAL;
    len = user_strnlen(path, (int)sizeof(kpath) - 1);
    if (len <= 0)
        return EFAULT;
    if (copy_from_user(kpath, path, (unsigned long)len + 1) < 0)
        return EFAULT;

    rc = ota_verify_package_kpath(kpath);
    return rc;
}

int ota_apply_package(const char *path) {
    char kpath[64];
    int len;
    int rc;

    if (!current_agent || !agent_has_cap(current_agent, CAP_FS))
        return EPERM;
    if (!path)
        return EINVAL;

    len = user_strnlen(path, (int)sizeof(kpath) - 1);
    if (len <= 0)
        return EFAULT;
    if (copy_from_user(kpath, path, (unsigned long)len + 1) < 0)
        return EFAULT;

    rc = ota_apply_package_kpath(kpath);
    return rc;
}
