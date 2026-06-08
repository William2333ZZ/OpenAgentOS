.set ALIGN,    1 << 0
.set MEMINFO,  1 << 1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
multiboot_header:
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .text
.code32
.global _start
_start:
    cli
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    jz 1f
    xorl %eax, %eax
    rep stosb
1:
    movl $stack_top, %esp
    call kmain
halt:
    hlt
    jmp halt

.bss
.align 16
stack_bottom:
    .skip 16384
.global stack_top
stack_top:
