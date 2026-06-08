.section .text
.globl trap_vector
.globl switch_to_agent

.equ TF_SIZE, 264
.equ TF_SEPC, 248
.equ TF_SSTATUS, 256
.equ SSTATUS_SPP, 1 << 8
.equ TRAP_KSTACK, 2048

.equ TRAP_STACK_SIZE, 8192
.equ TRAP_STACK_SHIFT, 13

.extern trap_stacks

# Per-hart trap stack: sp = trap_stacks + hartid * TRAP_STACK_SIZE + offset
.macro TRAP_STACK_PTR hart_reg, out
    la \out, trap_stacks
    slli t6, \hart_reg, TRAP_STACK_SHIFT
    add \out, \out, t6
.endm

# void agent_save_context(struct trapframe *tf)
.globl agent_save_context
agent_save_context:
    sd sp, 8(a0)
    sd gp, 16(a0)
    sd s0, 56(a0)
    sd s1, 64(a0)
    sd s2, 136(a0)
    sd s3, 144(a0)
    sd s4, 152(a0)
    sd s5, 160(a0)
    sd s6, 168(a0)
    sd s7, 176(a0)
    sd s8, 184(a0)
    sd s9, 192(a0)
    sd s10, 200(a0)
    sd s11, 208(a0)
    ret

# void switch_to_agent(struct trapframe *tf)
switch_to_agent:
    mv s2, a0
    ld ra, 0(s2)
    ld sp, 8(s2)
    ld gp, 16(s2)
    ld tp, 24(s2)
    ld t0, 32(s2)
    ld t1, 40(s2)
    ld t2, 48(s2)
    ld s0, 56(s2)
    ld a0, 72(s2)
    ld a1, 80(s2)
    ld a2, 88(s2)
    ld a3, 96(s2)
    ld a4, 104(s2)
    ld a5, 112(s2)
    ld a6, 120(s2)
    ld a7, 128(s2)
    ld s3, 144(s2)
    ld s4, 152(s2)
    ld s5, 160(s2)
    ld s6, 168(s2)
    ld s7, 176(s2)
    ld s8, 184(s2)
    ld s9, 192(s2)
    ld s10, 200(s2)
    ld s11, 208(s2)
    ld t3, 216(s2)
    ld t4, 224(s2)
    ld t5, 232(s2)
    ld t6, 240(s2)
    ld t0, TF_SSTATUS(s2)
    li t1, 256
    xori t1, t1, -1
    and t0, t0, t1
    ori t0, t0, 32
    lui t1, 0x4
    or t0, t0, t1
    csrw sstatus, t0
    ld t0, TF_SEPC(s2)
    csrw sepc, t0
    ld t1, 40(s2)
    ld s1, 64(s2)
    ld s2, 136(s2)
    sret

.macro SAVE reg, offset
    sd \reg, \offset(sp)
.endm

.macro LOAD reg, offset, base
    ld \reg, \offset(\base)
.endm

.macro SAVE_ALL
    SAVE ra, 0
    SAVE gp, 16
    SAVE tp, 24
    SAVE t0, 32
    SAVE t1, 40
    SAVE t2, 48
    SAVE s0, 56
    SAVE s1, 64
    SAVE a0, 72
    SAVE a1, 80
    SAVE a2, 88
    SAVE a3, 96
    SAVE a4, 104
    SAVE a5, 112
    SAVE a6, 120
    SAVE a7, 128
    SAVE s2, 136
    SAVE s3, 144
    SAVE s4, 152
    SAVE s5, 160
    SAVE s6, 168
    SAVE s7, 176
    SAVE s8, 184
    SAVE s9, 192
    SAVE s10, 200
    SAVE s11, 208
    SAVE t3, 216
    SAVE t4, 224
    SAVE t5, 232
    SAVE t6, 240
    csrr t6, sepc
    sd t6, TF_SEPC(sp)
    csrr t6, sstatus
    sd t6, TF_SSTATUS(sp)
.endm

    .align 4
trap_vector:
    csrr t0, sstatus
    andi t0, t0, SSTATUS_SPP
    bnez t0, kernel_trap

user_trap:
    mv t4, sp
    csrr t5, sscratch
    TRAP_STACK_PTR t5, sp
    li t6, TRAP_STACK_SIZE
    add sp, sp, t6
    addi sp, sp, -TF_SIZE
    sd t4, 8(sp)
    j save_done

kernel_trap:
    addi sp, sp, -TF_SIZE
    addi t4, sp, TF_SIZE
    sd t4, 8(sp)

save_done:
    SAVE_ALL
    mv a0, sp
    addi sp, sp, -2048
    call trap_handler
    li t1, 2048
    add sp, sp, t1

    mv s0, a0
    LOAD ra, 0, s0
    LOAD sp, 8, s0
    LOAD gp, 16, s0
    LOAD tp, 24, s0
    LOAD t0, 32, s0
    LOAD t1, 40, s0
    LOAD t2, 48, s0
    LOAD s1, 64, s0
    LOAD a0, 72, s0
    LOAD a1, 80, s0
    LOAD a2, 88, s0
    LOAD a3, 96, s0
    LOAD a4, 104, s0
    LOAD a5, 112, s0
    LOAD a6, 120, s0
    LOAD a7, 128, s0
    LOAD s2, 136, s0
    LOAD s3, 144, s0
    LOAD s4, 152, s0
    LOAD s5, 160, s0
    LOAD s6, 168, s0
    LOAD s7, 176, s0
    LOAD s8, 184, s0
    LOAD s9, 192, s0
    LOAD s10, 200, s0
    LOAD s11, 208, s0
    LOAD t3, 216, s0
    LOAD t4, 224, s0
    LOAD t5, 232, s0
    LOAD t6, 240, s0
    LOAD t0, TF_SSTATUS, s0
    li t1, 256
    xori t1, t1, -1
    and t0, t0, t1
    ori t0, t0, 32
    lui t1, 0x4
    or t0, t0, t1
    csrw sstatus, t0
    LOAD t0, TF_SEPC, s0
    csrw sepc, t0
    LOAD t1, 40, s0
    LOAD s0, 56, s0
    sret

.section .bss
.align 12
.globl trap_stacks
trap_stacks:
    .space 8192 * 4
