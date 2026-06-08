#include "catalog.h"
#include "../include/agentos.h"

void catalog_init(void) {}

int catalog_list(void) {
    return ENODEV;
}

int catalog_install(const char *name) {
    (void)name;
    return ENODEV;
}

int catalog_rollback(const char *name) {
    (void)name;
    return ENODEV;
}
