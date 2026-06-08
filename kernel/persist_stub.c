#include "persist.h"
#include "../include/agentos.h"

void persist_init(void) {
}

int persist_load(void) {
    return 0;
}

int persist_sync(void) {
    return ENODEV;
}

int persist_migrate(void) {
    return ENODEV;
}

int persist_ready(void) {
    return 0;
}
