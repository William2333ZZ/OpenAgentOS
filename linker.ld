OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    .text : {
        *(.text.init)
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.text .text.*)
    }

    . = ALIGN(16);
    _etext = .;

    .rodata : {
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.rodata .rodata.*)
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.srodata .srodata.*)
    }

    . = ALIGN(16);
    _erodata = .;

    .data : {
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.data .data.*)
    }

    .bss : {
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.bss .bss.*)
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(COMMON)
    }

    .sbss : {
        EXCLUDE_FILE(*demo_pipeline.o *demo_llm.o *demo_harness.o *demo_tools.o *demo_v1.o *demo_memory.o *demo_memory2.o *demo_orchestrator.o *demo_storage2.o *demo_tools2.o *demo_wrap.o *demo_vm3.o *demo_edge.o *demo_router.o *demo_load.o *demo_net.o *demo_net_prod.o *demo_ui.o *demo_console.o *demo_console_session.o *demo_console_orch.o *demo_console_pack.o *demo_catalog.o *demo_console_tenant.o *demo_console_audit.o *demo_console_namespace.o *demo_console_quota.o *demo_console_v7.o *demo_v040.o *demo_desktop.o *demo_box.o *demo_bench_ipc.o *demo_persist2.o *demo_smp.o *demo_ota.o *agent_svc.o *router_svc.o *network_svc.o *network_prod_svc.o *display_svc.o *input_svc.o *console_svc.o *shell_svc.o *orch_pipeline.o *storage_svc.o) *(.sbss .sbss.*)
    }

    . = ALIGN(16);
    _end = .;

    . = 0x80300000;
    _user_text_start = .;
    .user.text : {
        *demo_pipeline.o(.text .text.*)
        *demo_llm.o(.text .text.*)
        *demo_harness.o(.text .text.*)
        *demo_tools.o(.text .text.*)
        *demo_v1.o(.text .text.*)
        *demo_memory.o(.text .text.*)
        *demo_memory2.o(.text .text.*)
        *demo_orchestrator.o(.text .text.*)
        *demo_storage2.o(.text .text.*)
        *demo_tools2.o(.text .text.*)
        *demo_wrap.o(.text .text.*)
        *demo_vm3.o(.text .text.*)
        *demo_edge.o(.text .text.*)
        *demo_router.o(.text .text.*)
        *demo_load.o(.text .text.*)
        *demo_net.o(.text .text.*)
        *demo_net_prod.o(.text .text.*)
        *demo_ui.o(.text .text.*)
        *demo_console.o(.text .text.*)
        *demo_console_session.o(.text .text.*)
        *demo_console_orch.o(.text .text.*)
        *demo_console_pack.o(.text .text.*)
        *demo_catalog.o(.text .text.*)
        *demo_console_tenant.o(.text .text.*)
        *demo_console_audit.o(.text .text.*)
        *demo_console_namespace.o(.text .text.*)
        *demo_console_quota.o(.text .text.*)
        *demo_console_v7.o(.text .text.*)
        *demo_v040.o(.text .text.*)
        *demo_desktop.o(.text .text.*)
        *demo_box.o(.text .text.*)
        *demo_bench_ipc.o(.text .text.*)
        *demo_persist2.o(.text .text.*)
        *demo_smp.o(.text .text.*)
        *demo_ota.o(.text .text.*)
        *agent_svc.o(.text .text.*)
        *router_svc.o(.text .text.*)
        *network_svc.o(.text .text.*)
        *network_prod_svc.o(.text .text.*)
        *display_svc.o(.text .text.*)
        *input_svc.o(.text .text.*)
        *console_svc.o(.text .text.*)
        *shell_svc.o(.text .text.*)
        *orch_pipeline.o(.text .text.*)
        *storage_svc.o(.text .text.*)
    }
    _user_text_end = .;

    . = ALIGN(4096);
    .user.rodata : {
        *demo_pipeline.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_llm.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_harness.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_tools.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_v1.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_memory.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_memory2.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_orchestrator.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_storage2.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_tools2.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_wrap.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_vm3.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_edge.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_router.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_load.o(.rodata .rodata.* .srodata .srodata.* .data .data.*)
        *demo_net.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_net_prod.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_ui.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_session.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_orch.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_pack.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_catalog.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_tenant.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_audit.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_namespace.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_quota.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_console_v7.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_v040.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_desktop.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_box.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_bench_ipc.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_persist2.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_smp.o(.rodata .rodata.* .srodata .srodata.*)
        *demo_ota.o(.rodata .rodata.* .srodata .srodata.*)
        *agent_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *router_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *network_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *network_prod_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *display_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *input_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *console_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *shell_svc.o(.rodata .rodata.* .srodata .srodata.*)
        *orch_pipeline.o(.rodata .rodata.* .srodata .srodata.*)
        *storage_svc.o(.rodata .rodata.* .srodata .srodata.*)
    }
    _user_rodata_end = .;

    . = ALIGN(4096);
    _user_bss_start = .;
    .user.bss (NOLOAD) : {
        *demo_pipeline.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_llm.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_harness.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_tools.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_v1.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_memory.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_memory2.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_orchestrator.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_storage2.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_tools2.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_wrap.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_vm3.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_edge.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_router.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_load.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_net.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_net_prod.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_ui.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_session.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_orch.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_pack.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_catalog.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_tenant.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_audit.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_namespace.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_quota.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_console_v7.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_v040.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_desktop.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_box.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_bench_ipc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_persist2.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_smp.o(.bss .bss.* .sbss .sbss.* COMMON)
        *demo_ota.o(.bss .bss.* .sbss .sbss.* COMMON)
        *agent_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *router_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *network_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *network_prod_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *display_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *input_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *console_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *shell_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
        *orch_pipeline.o(.bss .bss.* .sbss .sbss.* COMMON)
        *storage_svc.o(.bss .bss.* .sbss .sbss.* COMMON)
    }
    _user_end = .;

    . = 0x80800000;
    _ptpool_start = .;
    .ptpool (NOLOAD) : {
        *(.ptpool)
    }
    _ptpool_end = .;
}
