#ifndef PERSIST_H
#define PERSIST_H

void persist_init(void);
int persist_load(void);
int persist_sync(void);
int persist_migrate(void);
int persist_ready(void);

#endif
