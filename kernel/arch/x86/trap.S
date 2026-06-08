.set TF_SIZE, 132
.set TF_RA, 0
.set TF_SP, 4
.set TF_GP, 8
.set TF_S0, 28
.set TF_A0, 36
.set TF_A1, 40
.set TF_A2, 44
.set TF_A3, 48
.set TF_A4, 52
.set TF_A7, 64
.set TF_T6, 120
.set TF_SEPC, 124
.set TF_SSTATUS, 128

.set USER_CS, 0x1b
.set USER_DS, 0x23
.set KERNEL_CS, 0x08
.set KERNEL_DS, 0x10

.section .text

.macro RESTORE_REGS tfreg
    movl TF_A7(\tfreg), %eax
    movl TF_GP(\tfreg), %ebx
    movl TF_A1(\tfreg), %ecx
    movl TF_A2(\tfreg), %edx
    movl TF_A3(\tfreg), %esi
    movl TF_S0(\tfreg), %ebp
    movl TF_A4(\tfreg), %edi
.endm

.macro RESTORE_USER tfreg
    movl $USER_DS, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl $USER_DS
    pushl TF_SP(\tfreg)
    pushl TF_SSTATUS(\tfreg)
    pushl $USER_CS
    pushl TF_SEPC(\tfreg)
    RESTORE_REGS \tfreg
    iret
.endm

.macro RESTORE_KERNEL tfreg
    movl $KERNEL_DS, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl TF_SSTATUS(\tfreg)
    pushl $KERNEL_CS
    pushl TF_SEPC(\tfreg)
    RESTORE_REGS \tfreg
    iret
.endm

.macro RESTORE_AND_IRET tfreg
    testl $3, TF_T6(\tfreg)
    jz 1f
    RESTORE_USER \tfreg
1:
    RESTORE_KERNEL \tfreg
.endm

.macro TRAP_ENTRY eip_off vec
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl %esp, %ebp
    subl $TF_SIZE, %esp
    movl \eip_off(%ebp), %eax
    movl %eax, TF_SEPC(%esp)
    movl (\eip_off + 4)(%ebp), %eax
    movl %eax, TF_T6(%esp)
    movl (\eip_off + 8)(%ebp), %eax
    movl %eax, TF_SSTATUS(%esp)
    movl (\eip_off + 12)(%ebp), %eax
    movl %eax, TF_SP(%esp)
    movl 40(%ebp), %eax
    movl %eax, TF_A7(%esp)
    movl 36(%ebp), %eax
    movl %eax, TF_GP(%esp)
    movl %eax, TF_A0(%esp)
    movl 32(%ebp), %eax
    movl %eax, TF_A1(%esp)
    movl 28(%ebp), %eax
    movl %eax, TF_A2(%esp)
    movl 24(%ebp), %eax
    movl %eax, TF_A3(%esp)
    movl 20(%ebp), %eax
    movl %eax, TF_A4(%esp)
    movl 16(%ebp), %eax
    movl %eax, TF_S0(%esp)
    movl $\vec, %eax
    movl %eax, TF_RA(%esp)
    movl $KERNEL_DS, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp
    call trap_handler
    addl $4, %esp
    movl %eax, %edi
    RESTORE_AND_IRET %edi
.endm

.globl trap_entry_gpf
trap_entry_gpf:
    TRAP_ENTRY 44, 0x0d

.globl trap_entry_page
trap_entry_page:
    TRAP_ENTRY 48, 0x0e

.globl trap_entry_timer
trap_entry_timer:
    TRAP_ENTRY 44, 0x20

.globl trap_entry_syscall
trap_entry_syscall:
    TRAP_ENTRY 44, 0x80

.section .bss
.p2align 12
.globl trap_stacks
trap_stacks:
    .space 8192
