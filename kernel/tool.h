#ifndef TOOL_H
#define TOOL_H

struct agent;

void tool_init(void);
int tool_dispatch(struct agent *a, int tool, long arg0, long arg1, long arg2);

#endif
