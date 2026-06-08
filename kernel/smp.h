#ifndef SMP_H
#define SMP_H

#define SMP_MAX_HARTS 4

#define ACLINT_MSWI_BASE 0x02000000UL

void smp_init(void);
void smp_ipi_init(void);
int smp_boot(unsigned long hartid, unsigned long dtb);
void smp_secondary_entry(unsigned long hartid);
int smp_online_count(void);
int smp_boot_hart(void);
void smp_wake_harts(int target);
unsigned long smp_hart_ticks(int hartid);
int smp_hart_ready(int hartid);
int smp_hart_id(void);
void smp_kick_cpu(int hart);
void smp_kick_others(int except_hart);
void smp_ipi_clear(int hart);
void smp_cpu_wait(void);
void smp_bsp_release(void);

#endif
