#ifndef ELFLOAD_H
#define ELFLOAD_H

#include "agent.h"

#define AGENT_ELF_MAX 8192

int agent_load_from_path(const char *path, const char *name, unsigned int caps);

#endif
