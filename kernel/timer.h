#ifndef TIMER_H
#define TIMER_H

void timer_init(void);
void timer_ack(void);
void timer_irq_unmask(void);
unsigned long timer_now(void);

#endif
