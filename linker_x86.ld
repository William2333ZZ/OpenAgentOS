OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PHDRS
{
    mbload PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 1M;

    .text : {
        *(.multiboot)
        *(.text.init)
        EXCLUDE_FILE(*demo_wrap*.o *demo_console*.o *demo_v0*.o *agent_svc_x86.o *router_svc_x86.o *display_svc_x86.o *input_svc_x86.o *console_svc_x86.o) *(.text .text.*)
    } :mbload

    _etext = .;

    .rodata : {
        EXCLUDE_FILE(*demo_wrap*.o *demo_console*.o *demo_v0*.o *agent_svc_x86.o *router_svc_x86.o *display_svc_x86.o *input_svc_x86.o *console_svc_x86.o) *(.rodata .rodata.*)
    } :mbload

    _erodata = .;

    .data : {
        EXCLUDE_FILE(*demo_wrap*.o *demo_console*.o *demo_v0*.o *agent_svc_x86.o *router_svc_x86.o *display_svc_x86.o *input_svc_x86.o *console_svc_x86.o) *(.data .data.*)
    } :mbload

    .bss : {
        __bss_start = .;
        EXCLUDE_FILE(*demo_wrap*.o *demo_console*.o *demo_v0*.o *agent_svc_x86.o *router_svc_x86.o *display_svc_x86.o *input_svc_x86.o *console_svc_x86.o) *(.bss .bss.*)
        EXCLUDE_FILE(*demo_wrap*.o *demo_console*.o *demo_v0*.o *agent_svc_x86.o *router_svc_x86.o *display_svc_x86.o *input_svc_x86.o *console_svc_x86.o) *(COMMON)
    } :mbload

    . = ALIGN(16);
    _end = .;

    . = ALIGN(4096);
    _user_text_start = .;
    .user.text : {
        *demo_wrap*.o(.text .text.*)
        *demo_console*.o(.text .text.*)
        *demo_v0*.o(.text .text.*)
        *agent_svc_x86.o(.text .text.*)
        *router_svc_x86.o(.text .text.*)
        *display_svc_x86.o(.text .text.*)
        *input_svc_x86.o(.text .text.*)
        *console_svc_x86.o(.text .text.*)
    } :mbload
    .user.rodata : {
        *demo_wrap*.o(.rodata .rodata.*)
        *demo_console*.o(.rodata .rodata.*)
        *demo_v0*.o(.rodata .rodata.*)
        *agent_svc_x86.o(.rodata .rodata.*)
        *router_svc_x86.o(.rodata .rodata.*)
        *display_svc_x86.o(.rodata .rodata.*)
        *input_svc_x86.o(.rodata .rodata.*)
        *console_svc_x86.o(.rodata .rodata.*)
    } :mbload
    .user.data : {
        *demo_wrap*.o(.data .data.*)
        *demo_console*.o(.data .data.*)
        *demo_v0*.o(.data .data.*)
        *agent_svc_x86.o(.data .data.*)
        *router_svc_x86.o(.data .data.*)
        *display_svc_x86.o(.data .data.*)
        *input_svc_x86.o(.data .data.*)
        *console_svc_x86.o(.data .data.*)
    } :mbload
    . = ALIGN(16);
    _user_bss_start = .;
    .user.bss : {
        *demo_wrap*.o(.bss .bss.*)
        *demo_wrap*.o(COMMON)
        *demo_console*.o(.bss .bss.*)
        *demo_console*.o(COMMON)
        *demo_v0*.o(.bss .bss.*)
        *demo_v0*.o(COMMON)
        *agent_svc_x86.o(.bss .bss.*)
        *agent_svc_x86.o(COMMON)
        *router_svc_x86.o(.bss .bss.*)
        *router_svc_x86.o(COMMON)
        *display_svc_x86.o(.bss .bss.*)
        *display_svc_x86.o(COMMON)
        *input_svc_x86.o(.bss .bss.*)
        *input_svc_x86.o(COMMON)
        *console_svc_x86.o(.bss .bss.*)
        *console_svc_x86.o(COMMON)
    } :mbload
    . = ALIGN(4096);
    _user_end = .;

    . = ALIGN(4096);
    _ptpool_start = .;
    .ptpool : {
        *(.ptpool)
    } :mbload
    _ptpool_end = .;

    __bss_end = _end;
}
