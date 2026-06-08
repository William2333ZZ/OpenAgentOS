.section .text.init
.globl _start
.extern smp_secondary_entry

_start:
    # a0 = hart id, a1 = dtb (from OpenSBI)
    bnez a0, secondary_boot

    la sp, kernel_stack_top
    csrw sscratch, zero
    call kmain
1:  wfi
    j 1b

secondary_boot:
    csrw sscratch, a0
    la sp, secondary_stack_top
    li t0, 16384
    addi t1, a0, -1
    mul t1, t1, t0
    sub sp, sp, t1
    call smp_secondary_entry
2:  wfi
    j 2b

.section .bss
.align 16
kernel_stack:
    .space 16384
.globl kernel_stack_top
kernel_stack_top:

secondary_stacks:
    .space 16384 * 3
.globl secondary_stack_top
secondary_stack_top:
