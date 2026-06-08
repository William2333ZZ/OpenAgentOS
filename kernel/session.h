#ifndef SESSION_H
#define SESSION_H

#include "agent.h"

int session_append(struct agent *a, const char *text);
int session_tail(struct agent *a, char *buf, int buflen);
int session_read(struct agent *a, char *buf, int buflen, int max_lines);
int session_compact(struct agent *a, int keep);

#endif
