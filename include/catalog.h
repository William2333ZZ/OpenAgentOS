#ifndef CATALOG_H
#define CATALOG_H

#define CATALOG_PATH_INDEX    "/agent/1/catalog/index"
#define CATALOG_PATH_ACTIVE   "/agent/1/catalog/active"
#define CATALOG_PATH_ROLLBACK "/agent/1/catalog/rollback"

void catalog_init(void);
int catalog_list(void);
int catalog_install(const char *name);
int catalog_rollback(const char *name);

#endif
