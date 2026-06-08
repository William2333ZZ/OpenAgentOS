CROSS   := riscv64-unknown-elf
CC      := $(CROSS)-gcc
QEMU    := qemu-system-riscv64

CFLAGS  := -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany
CFLAGS  += -ffreestanding -nostdlib -nostartfiles
CFLAGS  += -Wall -Wextra -O2
CFLAGS  += -Iinclude -Ikernel -Iuser

X86_CC      := i686-elf-gcc
X86_CFLAGS  := -ffreestanding -nostdlib -nostartfiles \
               -Wall -Wextra -O2 -Iinclude -Ikernel -Iuser -DPLATFORM_X86_64_PC
X86_LDFLAGS := -T linker_x86.ld -nostdlib -static -no-pie

MBEDTLS_DIR := third_party/mbedtls
MBEDTLS_CFLAGS := $(CFLAGS) -I$(MBEDTLS_DIR)/include -Ikernel -Igenerated \
  -DMBEDTLS_CONFIG_FILE=\"mbedtls_config_agentos.h\"
MBEDTLS_LIBS := aes asn1parse asn1write bignum bignum_core cipher cipher_wrap constant_time \
  ctr_drbg ecp ecp_curves ecdh entropy error gcm md oid pk pk_wrap pk_ecc pkparse \
  platform_util rsa rsa_alt_helpers sha256 ssl_ciphersuites ssl_client ssl_msg ssl_tls \
  ssl_tls12_client x509 x509_crt
MBEDTLS_OBJS := $(addprefix $(MBEDTLS_DIR)/library/, $(addsuffix .o,$(MBEDTLS_LIBS)))

PLATFORM_RV_OBJS := kernel/platform_rv.o kernel/platform_riscv_virt.o kernel/smp.o kernel/trap_init.o kernel/spinlock.o kernel/ota_stub.o kernel/catalog_stub.o kernel/tenant_stub.o kernel/virtio_ui_stub.o kernel/sched_syscall.o kernel/arch/riscv/trap_handler.o kernel/arch/riscv/agent_arch.o kernel/fleet_stub.o kernel/policy_stub.o kernel/remote_stub.o kernel/mesh_stub.o

V7_RV_PLATFORM_OBJS := $(filter-out kernel/fleet_stub.o kernel/policy_stub.o kernel/remote_stub.o kernel/mesh_stub.o,$(PLATFORM_RV_OBJS)) \
                       kernel/fleet.o kernel/policy.o kernel/remote.o kernel/mesh.o

LDFLAGS := -T linker.ld -nostdlib -nostartfiles -static

COMMON_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
                kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
                kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
                kernel/vm.o kernel/virtio.o kernel/llm.o kernel/session.o \
                kernel/persist_stub.o kernel/elfload_stub.o

V1_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/llm_stub.o kernel/elfload_stub.o kernel/kernel_v1.o user/demo_v1.o user/agent_svc.o

MEMORY_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/virtio.o kernel/llm.o kernel/elfload_stub.o kernel/kernel_memory.o user/demo_memory.o user/agent_svc.o

ORCH_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio.o kernel/llm.o kernel/session.o kernel/persist_stub.o \
           kernel/elfload_stub.o kernel/kernel_orchestrator.o user/demo_orchestrator.o user/agent_svc.o

MEMORY2_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/virtio.o kernel/llm.o kernel/elfload_stub.o kernel/kernel_memory2.o user/demo_memory2.o user/agent_svc.o

STORAGE2_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/llm_stub.o kernel/persist_stub.o \
           kernel/elfload_stub.o kernel/kernel_storage2.o user/demo_storage2.o user/agent_svc.o

TOOLS2_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/llm_stub.o kernel/persist_stub.o \
           kernel/elfload_stub.o kernel/kernel_tools2.o user/demo_tools2.o

WRAP_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/llm_stub.o kernel/elfload_stub.o kernel/kernel_wrap.o user/demo_wrap.o

VM3_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/llm_stub.o kernel/persist_stub.o \
           kernel/elfload_stub.o kernel/kernel_vm3.o user/demo_vm3.o

EDGE_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/virtio.o kernel/llm.o kernel/elfload_stub.o kernel/kernel_edge.o user/demo_edge.o \
           user/agent_svc.o user/router_svc.o

ROUTER_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/llm.o \
           kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_router.o user/demo_router.o user/agent_svc.o user/router_svc.o

NET_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_net.o \
           kernel/http.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_net.o user/demo_net.o user/agent_svc.o user/network_svc.o

NET_PROD_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_net.o kernel/netstack.o \
           kernel/http.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_net_prod.o user/demo_net_prod.o user/agent_svc.o user/network_prod_svc.o

LLM_NET_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/string.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio_net.o kernel/netstack.o \
           kernel/tls_client.o kernel/llm_deepseek.o kernel/mbedtls_port.o \
           $(MBEDTLS_OBJS) kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/llm_net.o kernel/kernel_llm_net.o user/demo_llm.o

UI_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_ui.o user/demo_ui.o user/agent_svc.o user/display_svc.o user/input_svc.o

CONSOLE_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm-faux.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console.o user/demo_console.o user/agent_svc.o user/router_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_SESSION_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm-faux.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_session.o user/demo_console_session.o user/agent_svc.o user/router_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_ORCH_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm-faux.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_orch.o user/demo_console_orch.o user/orch_pipeline.o user/storage_svc.o \
           user/agent_svc.o user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_PACK_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm_stub.o kernel/elfload.o kernel/persist_stub.o \
           kernel/kernel_console_pack.o user/demo_console_pack.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CATALOG_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm_stub.o kernel/elfload.o kernel/ota.o kernel/catalog.o kernel/persist_stub.o \
           kernel/kernel_catalog.o user/demo_catalog.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_TENANT_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/tenant.o kernel/virtio.o kernel/virtio_gpu.o \
           kernel/virtio_input.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_tenant.o user/demo_console_tenant.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_AUDIT_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/tenant.o kernel/virtio.o kernel/virtio_gpu.o \
           kernel/virtio_input.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_audit.o user/demo_console_audit.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_NAMESPACE_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/tenant.o kernel/virtio.o kernel/virtio_gpu.o \
           kernel/virtio_input.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_namespace.o user/demo_console_namespace.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_QUOTA_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/tenant.o kernel/virtio.o kernel/virtio_gpu.o \
           kernel/virtio_input.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/kernel_console_quota.o user/demo_console_quota.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_V7_BASE_OBJS := $(filter-out kernel/tenant_stub.o,$(V7_RV_PLATFORM_OBJS)) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/tenant.o kernel/virtio.o kernel/virtio_gpu.o \
           kernel/virtio_input.o kernel/llm_stub.o kernel/persist_stub.o kernel/elfload_stub.o \
           kernel/netstack_stub.o \
           kernel/kernel_console_v7.o user/demo_console_v7.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/console_svc.o

CONSOLE_V040_RV_OBJS := $(filter-out kernel/kernel_console_v7.o user/demo_console_v7.o kernel/http-faux.o kernel/netstack_stub.o,$(CONSOLE_V7_BASE_OBJS)) \
           kernel/http.o kernel/netstack.o kernel/virtio_net.o kernel/kernel_v040_riscv.o user/demo_v040.o

CONSOLE_V050_RV_OBJS := $(filter-out kernel/kernel_v040_riscv.o user/demo_v040.o kernel/llm_stub.o,$(CONSOLE_V040_RV_OBJS)) \
           kernel/llm_net.o kernel/llm_deepseek.o kernel/tls_client.o kernel/mbedtls_port.o kernel/string.o \
           $(MBEDTLS_OBJS) \
           kernel/kernel_v050_riscv.o user/demo_v050.o user/router_svc.o

DESKTOP_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/virtio.o kernel/virtio_gpu.o kernel/virtio_input.o \
           kernel/llm_stub.o kernel/elfload.o kernel/persist_stub.o \
           kernel/kernel_desktop.o user/demo_desktop.o user/agent_svc.o \
           user/display_svc.o user/input_svc.o user/shell_svc.o

BOX_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/llm_stub.o kernel/elfload.o kernel/persist_stub.o \
           kernel/kernel_box.o user/demo_console_pack.o user/agent_svc.o user/console_svc.o

BENCH_IPC_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/session.o kernel/llm_stub.o kernel/elfload_stub.o kernel/persist_stub.o \
           kernel/kernel_bench_ipc.o user/demo_bench_ipc.o

LOAD_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/elfload.o kernel/llm_stub.o \
           kernel/kernel_load.o user/demo_load.o

PERSIST2_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/llm_stub.o kernel/elfload_stub.o \
           kernel/kernel_persist2.o user/demo_persist2.o

SMP_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/persist_stub.o kernel/session.o kernel/llm_stub.o \
           kernel/elfload_stub.o kernel/kernel_smp.o user/demo_smp.o

OTA_BASE_OBJS := $(PLATFORM_RV_OBJS) kernel/entry.o kernel/trap.o kernel/uart.o kernel/printf.o \
           kernel/halt.o kernel/mem.o kernel/ipc.o kernel/uaccess.o kernel/ramfs.o \
           kernel/tool.o kernel/audit.o kernel/namespace.o kernel/quota.o kernel/http-faux.o kernel/agent.o kernel/sched.o kernel/timer.o \
           kernel/vm.o kernel/virtio_blk.o kernel/persist.o kernel/session.o \
           kernel/elfload.o kernel/llm_stub.o \
           kernel/kernel_ota.o user/demo_ota.o

.PHONY: all clean run run-harness run-tools run-llm run-llm-faux run-v1 check-v1 \
        run-memory run-memory-faux check-memory memory memory2 \
        run-memory2 run-memory2-faux check-memory2 \
        run-orchestrator run-orchestrator-faux check-orchestrator orchestrator \
        run-storage2 check-storage2 storage2 \
        run-tools2 check-tools2 tools2 \
        run-wrap check-wrap wrap \
        run-vm3 check-vm3 vm3 \
        run-edge run-edge-faux check-edge edge \
        run-router run-router-faux check-router router \
        run-net run-net-faux check-net net \
        run-net-prod check-net-prod net-prod \
        run-llm-net check-llm-net llm-net \
        run-ui run-ui-console check-ui check-ui-console ui ui-console \
        run-console check-console console \
        run-console-session check-console-session console-session \
        run-console-orch check-console-orch console-orch \
        run-console-pack check-console-pack console-pack \
        run-catalog check-catalog catalog \
        run-console-tenant check-console-tenant console-tenant \
        run-console-audit check-console-audit console-audit \
        run-console-namespace check-console-namespace console-namespace \
        run-console-quota check-console-quota console-quota \
        run-desktop check-desktop check-desktop-console desktop desktop-console \
        run-box check-box box bench-ipc \
        run-persist2 check-persist2 persist2 \
        run-load check-load load worker.agent \
        platform check-platform run-x86-smoke check-wrap-x86 check-console-x86 \
        check-console-x86-tenant check-console-x86-audit check-console-x86-namespace \
        check-console-x86-all check-v0.1-beta check-v0.2-rc check-0.1.0 check-0.2.0 \
        check-0.3.0 check-0.4.0 check-0.5.0 check-remote-console check-llm-console \
        check-console-v7 check-fleet-x86 \
        run-x86-v01-beta run-x86-v02-rc run-x86-0.3.0 run-x86-0.4.0 run-riscv-0.4.0 run-riscv-0.5.0 \
        run-smp check-smp smp \
        run-ota check-ota ota \
        pipeline llm harness tools v1

all: kernel.elf kernel-harness.elf kernel-tools.elf kernel-llm.elf kernel-llm-faux.elf \
     kernel-v1.elf kernel-memory.elf kernel-memory-faux.elf \
     kernel-memory2.elf kernel-memory2-faux.elf \
     kernel-orchestrator.elf kernel-orchestrator-faux.elf \
     kernel-storage2.elf kernel-tools2.elf kernel-wrap.elf kernel-vm3.elf \
     kernel-edge.elf kernel-edge-faux.elf \
     kernel-router.elf kernel-router-faux.elf \
     kernel-net.elf kernel-net-faux.elf kernel-net-prod.elf kernel-llm-net.elf kernel-ui.elf \
     kernel-console.elf kernel-console-session.elf kernel-console-orch.elf \
     kernel-persist2.elf \
     kernel-smp.elf \
     kernel-ota.elf kernel-ota-old.elf \
     kernel-load.elf worker.agent

kernel.elf: $(COMMON_OBJS) kernel-pipeline.o user/demo_pipeline.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(COMMON_OBJS) kernel-pipeline.o user/demo_pipeline.o

kernel-harness.elf: $(COMMON_OBJS) kernel-harness.o user/demo_harness.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(COMMON_OBJS) kernel-harness.o user/demo_harness.o

kernel-tools.elf: $(COMMON_OBJS) kernel-tools.o user/demo_tools.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(COMMON_OBJS) kernel-tools.o user/demo_tools.o

kernel-llm.elf: $(COMMON_OBJS) kernel-llm.o user/demo_llm.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(COMMON_OBJS) kernel-llm.o user/demo_llm.o

kernel-llm-faux.elf: $(COMMON_OBJS) kernel-llm-faux.o user/demo_llm.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(COMMON_OBJS) kernel-llm-faux.o user/demo_llm.o

kernel-v1.elf: $(V1_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(V1_OBJS)

kernel-memory.elf: $(MEMORY_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(MEMORY_BASE_OBJS)

kernel-memory-faux.elf: $(filter-out kernel/llm.o,$(MEMORY_BASE_OBJS)) kernel/llm-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/llm.o,$(MEMORY_BASE_OBJS)) kernel/llm-faux.o

kernel-memory2.elf: $(MEMORY2_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(MEMORY2_BASE_OBJS)

kernel-memory2-faux.elf: $(filter-out kernel/llm.o,$(MEMORY2_BASE_OBJS)) kernel/llm-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/llm.o,$(MEMORY2_BASE_OBJS)) kernel/llm-faux.o

kernel-orchestrator.elf: $(ORCH_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(ORCH_BASE_OBJS)

kernel-orchestrator-faux.elf: $(filter-out kernel/llm.o,$(ORCH_BASE_OBJS)) kernel/llm-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/llm.o,$(ORCH_BASE_OBJS)) kernel/llm-faux.o

kernel-storage2.elf: $(STORAGE2_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(STORAGE2_BASE_OBJS)

kernel-tools2.elf: $(TOOLS2_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(TOOLS2_BASE_OBJS)

kernel-wrap.elf: $(WRAP_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(WRAP_BASE_OBJS)

kernel-vm3.elf: $(VM3_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(VM3_BASE_OBJS)

kernel-edge.elf: $(EDGE_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(EDGE_BASE_OBJS)

kernel-edge-faux.elf: $(filter-out kernel/llm.o,$(EDGE_BASE_OBJS)) kernel/llm-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/llm.o,$(EDGE_BASE_OBJS)) kernel/llm-faux.o

kernel-router.elf: $(ROUTER_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(ROUTER_BASE_OBJS)

kernel-router-faux.elf: $(filter-out kernel/llm.o,$(ROUTER_BASE_OBJS)) kernel/llm-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/llm.o,$(ROUTER_BASE_OBJS)) kernel/llm-faux.o

kernel-net.elf: $(NET_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(NET_BASE_OBJS)

kernel-net-faux.elf: $(filter-out kernel/http.o kernel/virtio.o,$(NET_BASE_OBJS)) kernel/http-faux.o linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/http.o kernel/virtio.o,$(NET_BASE_OBJS)) kernel/http-faux.o

kernel-net-prod.elf: $(NET_PROD_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(NET_PROD_BASE_OBJS)

generated/deepseek_key.h:
	chmod +x scripts/mk-deepseek-key.sh
	./scripts/mk-deepseek-key.sh

generated/deepseek_host.h:
	chmod +x scripts/mk-deepseek-host.sh
	./scripts/mk-deepseek-host.sh

kernel-llm-net.elf: generated/deepseek_key.h generated/deepseek_host.h $(LLM_NET_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(LLM_NET_BASE_OBJS)

kernel-ui.elf: $(UI_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(UI_BASE_OBJS)

kernel-console.elf: $(CONSOLE_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_BASE_OBJS)

kernel-console-session.elf: $(CONSOLE_SESSION_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_SESSION_BASE_OBJS)

kernel-console-orch.elf: $(CONSOLE_ORCH_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_ORCH_BASE_OBJS)

kernel-console-pack.elf: $(CONSOLE_PACK_BASE_OBJS) linker.ld user/worker_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_PACK_BASE_OBJS)

kernel-catalog.elf: $(filter-out kernel/ota_stub.o kernel/catalog_stub.o,$(CATALOG_BASE_OBJS)) linker.ld user/worker_ota_v1_elf.inc user/worker_ota_v2_pkg.inc
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/ota_stub.o kernel/catalog_stub.o,$(CATALOG_BASE_OBJS))

kernel-console-tenant.elf: $(filter-out kernel/tenant_stub.o,$(CONSOLE_TENANT_BASE_OBJS)) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/tenant_stub.o,$(CONSOLE_TENANT_BASE_OBJS))

kernel-console-audit.elf: $(filter-out kernel/tenant_stub.o,$(CONSOLE_AUDIT_BASE_OBJS)) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/tenant_stub.o,$(CONSOLE_AUDIT_BASE_OBJS))

kernel-console-namespace.elf: $(filter-out kernel/tenant_stub.o kernel/namespace_stub.o,$(CONSOLE_NAMESPACE_BASE_OBJS)) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/tenant_stub.o kernel/namespace_stub.o,$(CONSOLE_NAMESPACE_BASE_OBJS))

kernel-console-quota.elf: $(filter-out kernel/tenant_stub.o kernel/namespace_stub.o kernel/quota_stub.o,$(CONSOLE_QUOTA_BASE_OBJS)) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/tenant_stub.o kernel/namespace_stub.o kernel/quota_stub.o,$(CONSOLE_QUOTA_BASE_OBJS))

kernel-console-v7.elf: $(CONSOLE_V7_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_V7_BASE_OBJS)

kernel-console-v040.elf: $(CONSOLE_V040_RV_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_V040_RV_OBJS)

kernel-console-v050.elf: generated/deepseek_key.h generated/deepseek_host.h $(CONSOLE_V050_RV_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(CONSOLE_V050_RV_OBJS)

kernel-desktop.elf: $(DESKTOP_BASE_OBJS) linker.ld user/worker_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(DESKTOP_BASE_OBJS)

kernel-box.elf: $(BOX_BASE_OBJS) linker.ld user/worker_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(BOX_BASE_OBJS)

kernel-bench-ipc.elf: $(BENCH_IPC_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(BENCH_IPC_BASE_OBJS)

kernel-persist2.elf: $(PERSIST2_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(PERSIST2_BASE_OBJS)

kernel-smp.elf: $(SMP_BASE_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(SMP_BASE_OBJS)

kernel-ota.elf: $(filter-out kernel/ota_stub.o,$(OTA_BASE_OBJS)) kernel/ota.o linker.ld user/worker_ota_v2_pkg.inc user/worker_ota_v1_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(filter-out kernel/ota_stub.o,$(OTA_BASE_OBJS)) kernel/ota.o

kernel-ota-old.elf: $(OTA_BASE_OBJS) linker.ld user/worker_ota_v2_pkg.inc user/worker_ota_v1_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(OTA_BASE_OBJS)

worker-ota-v1.agent: user/worker_ota_v1.o linker-agent.ld
	$(CC) $(CFLAGS) -T linker-agent.ld -nostdlib -static -o $@ user/worker_ota_v1.o

worker-ota-v2.agent: user/worker_ota_v2.o linker-agent.ld
	$(CC) $(CFLAGS) -T linker-agent.ld -nostdlib -static -o $@ user/worker_ota_v2.o

worker-ota-v2.agentpkg: worker-ota-v2.agent scripts/mk-agentpkg.py
	python3 scripts/mk-agentpkg.py worker-ota-v2.agent worker-ota-v2.agentpkg

user/worker_ota_v1_elf.inc: worker-ota-v1.agent
	xxd -i worker-ota-v1.agent > user/worker_ota_v1_elf.inc.tmp
	sed -e 's/worker_ota_v1_agent/worker_ota_v1_elf/g' \
	    -e 's/unsigned char worker_ota_v1_elf/const unsigned char worker_ota_v1_elf/g' \
	    -e 's/unsigned int worker_ota_v1_elf_len/const unsigned int worker_ota_v1_elf_len/g' \
	    user/worker_ota_v1_elf.inc.tmp > user/worker_ota_v1_elf.inc
	rm -f user/worker_ota_v1_elf.inc.tmp

user/worker_ota_v2_pkg.inc: worker-ota-v2.agentpkg
	xxd -i worker-ota-v2.agentpkg > user/worker_ota_v2_pkg.inc.tmp
	sed -e 's/worker_ota_v2_agentpkg/worker_ota_v2_pkg/g' \
	    -e 's/unsigned char worker_ota_v2_pkg/const unsigned char worker_ota_v2_pkg/g' \
	    -e 's/unsigned int worker_ota_v2_pkg_len/const unsigned int worker_ota_v2_pkg_len/g' \
	    user/worker_ota_v2_pkg.inc.tmp > user/worker_ota_v2_pkg.inc
	rm -f user/worker_ota_v2_pkg.inc.tmp

kernel-load.elf: $(LOAD_BASE_OBJS) linker.ld user/worker_elf.inc
	$(CC) $(LDFLAGS) -o $@ $(LOAD_BASE_OBJS)

worker.agent: user/worker_load.o linker-agent.ld
	$(CC) $(CFLAGS) -T linker-agent.ld -nostdlib -static -o $@ user/worker_load.o

user/worker_elf.inc: worker.agent
	xxd -i worker.agent > user/worker_elf.inc.tmp
	sed -e 's/worker_agent/worker_elf/g' \
	    -e 's/unsigned char worker_elf/const unsigned char worker_elf/g' \
	    -e 's/unsigned int worker_elf_len/const unsigned int worker_elf_len/g' \
	    user/worker_elf.inc.tmp > user/worker_elf.inc
	rm -f user/worker_elf.inc.tmp

kernel-pipeline.o: kernel/kernel.c
	$(CC) $(CFLAGS) -DINIT_AGENT=init_agent -c -o $@ $<

kernel-harness.o: kernel/kernel.c
	$(CC) $(CFLAGS) -DINIT_AGENT=init_harness_agent -c -o $@ $<

kernel-tools.o: kernel/kernel.c
	$(CC) $(CFLAGS) -DINIT_AGENT=init_tools_agent -c -o $@ $<

kernel-llm.o: kernel/kernel.c
	$(CC) $(CFLAGS) -DINIT_AGENT=init_llm_agent -DENABLE_VIRTIO -c -o $@ $<

kernel-llm-faux.o: kernel/kernel.c
	$(CC) $(CFLAGS) -DINIT_AGENT=init_llm_agent -DLLM_FAUX -c -o $@ $<

kernel/llm_net.o: kernel/llm.c
	$(CC) $(CFLAGS) -DLLM_NATIVE_NET -c -o $@ $<

kernel/kernel_llm_net.o: kernel/kernel_llm_net.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/tls_client.o: kernel/tls_client.c
	$(CC) $(MBEDTLS_CFLAGS) -c -o $@ $<

kernel/llm_deepseek.o: kernel/llm_deepseek.c generated/deepseek_key.h
	$(CC) $(CFLAGS) -Igenerated -c -o $@ $<

kernel/mbedtls_port.o: kernel/mbedtls_port.c
	$(CC) $(MBEDTLS_CFLAGS) -c -o $@ $<

$(MBEDTLS_DIR)/library/%.o: $(MBEDTLS_DIR)/library/%.c
	$(CC) $(MBEDTLS_CFLAGS) -c -o $@ $<

pipeline: kernel.elf
harness: kernel-harness.elf
tools: kernel-tools.elf
llm: kernel-llm.elf
v1: kernel-v1.elf
memory: kernel-memory.elf
memory2: kernel-memory2.elf
orchestrator: kernel-orchestrator.elf
storage2: kernel-storage2.elf
tools2: kernel-tools2.elf
wrap: kernel-wrap.elf
vm3: kernel-vm3.elf
edge: kernel-edge.elf
router: kernel-router.elf
load: kernel-load.elf
persist2: kernel-persist2.elf
smp: kernel-smp.elf
ota: kernel-ota.elf
net: kernel-net.elf

kernel/kernel_edge.o: kernel/kernel_edge.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_router.o: kernel/kernel_router.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_net.o: kernel/kernel_net.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_persist2.o: kernel/kernel_persist2.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_smp.o: kernel/kernel_smp.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/smp.o: kernel/smp.c kernel/smp.h
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/ota.o: kernel/ota.c include/ota.h
	$(CC) $(CFLAGS) -c -o $@ kernel/ota.c

kernel/catalog.o: kernel/catalog.c include/catalog.h include/ota.h
	$(CC) $(CFLAGS) -c -o $@ kernel/catalog.c

kernel/tenant.o: kernel/tenant.c include/tenant.h
	$(CC) $(CFLAGS) -c -o $@ kernel/tenant.c

kernel/tenant_stub.o: kernel/tenant_stub.c include/tenant.h
	$(CC) $(CFLAGS) -c -o $@ kernel/tenant_stub.c

kernel/audit.o: kernel/audit.c include/audit.h
	$(CC) $(CFLAGS) -c -o $@ kernel/audit.c

kernel/namespace.o: kernel/namespace.c include/namespace.h
	$(CC) $(CFLAGS) -c -o $@ kernel/namespace.c

kernel/namespace_stub.o: kernel/namespace_stub.c include/namespace.h
	$(CC) $(CFLAGS) -c -o $@ kernel/namespace_stub.c

kernel/quota.o: kernel/quota.c include/quota.h
	$(CC) $(CFLAGS) -c -o $@ kernel/quota.c

kernel/quota_stub.o: kernel/quota_stub.c include/quota.h
	$(CC) $(CFLAGS) -c -o $@ kernel/quota_stub.c

kernel/fleet.o: kernel/fleet.c include/fleet.h
	$(CC) $(CFLAGS) -c -o $@ kernel/fleet.c

kernel/fleet_stub.o: kernel/fleet_stub.c include/fleet.h
	$(CC) $(CFLAGS) -c -o $@ kernel/fleet_stub.c

kernel/policy.o: kernel/policy.c include/policy.h
	$(CC) $(CFLAGS) -c -o $@ kernel/policy.c

kernel/policy_stub.o: kernel/policy_stub.c include/policy.h
	$(CC) $(CFLAGS) -c -o $@ kernel/policy_stub.c

kernel/remote.o: kernel/remote.c include/remote.h
	$(CC) $(CFLAGS) -c -o $@ kernel/remote.c

kernel/remote_stub.o: kernel/remote_stub.c include/remote.h
	$(CC) $(CFLAGS) -c -o $@ kernel/remote_stub.c

kernel/mesh.o: kernel/mesh.c include/mesh.h
	$(CC) $(CFLAGS) -c -o $@ kernel/mesh.c

kernel/mesh_stub.o: kernel/mesh_stub.c include/mesh.h
	$(CC) $(CFLAGS) -c -o $@ kernel/mesh_stub.c

kernel/audit_stub.o: kernel/audit_stub.c include/audit.h
	$(CC) $(CFLAGS) -c -o $@ kernel/audit_stub.c

kernel/ota_stub.o: kernel/ota_stub.c include/ota.h
	$(CC) $(CFLAGS) -c -o $@ kernel/ota_stub.c

kernel/catalog_stub.o: kernel/catalog_stub.c include/catalog.h
	$(CC) $(CFLAGS) -c -o $@ kernel/catalog_stub.c

kernel/kernel_ota.o: kernel/kernel_ota.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/http-faux.o: kernel/http.c
	$(CC) $(CFLAGS) -DHTTP_FAUX -c -o $@ $<

kernel/kernel_load.o: kernel/kernel_load.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/elfload.o: kernel/elfload.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/elfload_stub.o: kernel/elfload_stub.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_v1.o: kernel/kernel_v1.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_memory.o: kernel/kernel_memory.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_memory2.o: kernel/kernel_memory2.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_orchestrator.o: kernel/kernel_orchestrator.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_storage2.o: kernel/kernel_storage2.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_tools2.o: kernel/kernel_tools2.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/kernel_wrap.o: kernel/kernel_wrap.c
	$(CC) $(CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_vm3.o: kernel/kernel_vm3.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/llm-faux.o: kernel/llm.c
	$(CC) $(CFLAGS) -DLLM_FAUX -c -o $@ $<

kernel/arch/riscv/%.o: kernel/arch/riscv/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/arch/x86/%.o: kernel/arch/x86/%.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/%.o: kernel/%.c
	@if echo '$*' | grep -q '/'; then echo "missing arch rule for kernel/$*"; exit 1; fi
	$(CC) $(CFLAGS) -c -o $@ $<

user/agent_svc.o: user/agent_svc.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_pipeline.o: user/demo_pipeline.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_harness.o: user/demo_harness.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_tools.o: user/demo_tools.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_llm.o: user/demo_llm.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_v1.o: user/demo_v1.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_memory.o: user/demo_memory.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_memory2.o: user/demo_memory2.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_orchestrator.o: user/demo_orchestrator.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_storage2.o: user/demo_storage2.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_tools2.o: user/demo_tools2.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_wrap.o: user/demo_wrap.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_vm3.o: user/demo_vm3.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_edge.o: user/demo_edge.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_router.o: user/demo_router.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/router_svc.o: user/router_svc.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/network_svc.o: user/network_svc.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_net.o: user/demo_net.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_persist2.o: user/demo_persist2.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_smp.o: user/demo_smp.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/worker_ota_v1.o: user/worker_ota_v1.c include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/worker_ota_v2.o: user/worker_ota_v2.c include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_ota.o: user/demo_ota.c user/worker_ota_v1_elf.inc user/worker_ota_v2_pkg.inc user/libagent.h include/agentos.h include/ota.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_ota.c

user/worker_load.o: user/worker_load.c include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_load.o: user/demo_load.c user/worker_elf.inc user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_load.c

user/demo_console_pack.o: user/demo_console_pack.c user/worker_elf.inc user/libagent.h include/agentos.h include/ota.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_pack.c

user/demo_catalog.o: user/demo_catalog.c user/worker_ota_v1_elf.inc user/worker_ota_v2_pkg.inc user/libagent.h include/agentos.h include/ota.h include/catalog.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_catalog.c

user/demo_console_tenant.o: user/demo_console_tenant.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_tenant.c

user/demo_console_audit.o: user/demo_console_audit.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_audit.c

user/demo_console_namespace.o: user/demo_console_namespace.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_namespace.c

user/demo_console_quota.o: user/demo_console_quota.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_quota.c

user/demo_console_v7.o: user/demo_console_v7.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_console_v7.c

kernel/kernel_console_v7.o: kernel/kernel_console_v7.c
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_desktop.o: user/demo_desktop.c user/worker_elf.inc user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_desktop.c

user/demo_bench_ipc.o: user/demo_bench_ipc.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_bench_ipc.c

run: kernel.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel.elf

run-harness: kernel-harness.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-harness.elf

run-tools: kernel-tools.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-tools.elf

run-llm:
	./scripts/run-llm.sh

run-llm-faux: kernel-llm-faux.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-llm-faux.elf

run-v1:
	./scripts/run-v1.sh

check-v1:
	./scripts/check-v1.sh

run-memory:
	./scripts/run-memory.sh

run-memory-faux: kernel-memory-faux.elf
	AGENTOS_DISK="$${AGENTOS_DISK:-/tmp/agentos-memory-faux.img}" \
	./scripts/run-memory-faux.sh

check-memory:
	./scripts/check-memory.sh

run-memory2-faux: kernel-memory2-faux.elf
	AGENTOS_DISK="$${AGENTOS_DISK:-/tmp/agentos-memory2-faux.img}" \
	./scripts/run-memory2-faux.sh

check-memory2:
	./scripts/check-memory2.sh

run-orchestrator:
	./scripts/run-orchestrator.sh

run-orchestrator-faux: kernel-orchestrator-faux.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-orchestrator-faux.elf

check-orchestrator:
	./scripts/check-orchestrator.sh

run-storage2: kernel-storage2.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-storage2.elf

check-storage2:
	./scripts/check-storage2.sh

run-tools2: kernel-tools2.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-tools2.elf

check-tools2:
	./scripts/check-tools2.sh

run-wrap: kernel-wrap.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -drive if=none,id=blk,format=raw,file=$${AGENTOS_DISK:-/tmp/agentos-wrap.img} \
	  -device virtio-blk-device,drive=blk \
	  -kernel kernel-wrap.elf

check-wrap:
	./scripts/check-wrap.sh

run-vm3: kernel-vm3.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-vm3.elf

check-vm3:
	./scripts/check-vm3.sh

run-edge: kernel-edge.elf
	AGENTOS_DISK=$${AGENTOS_DISK:-/tmp/agentos-edge.img} \
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -drive if=none,id=blk,format=raw,file=$${AGENTOS_DISK:-/tmp/agentos-edge.img} \
	  -device virtio-blk-device,drive=blk \
	  -kernel kernel-edge.elf

run-edge-faux: kernel-edge-faux.elf
	AGENTOS_DISK=$${AGENTOS_DISK:-/tmp/agentos-edge-faux.img} \
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -drive if=none,id=blk,format=raw,file=$${AGENTOS_DISK:-/tmp/agentos-edge-faux.img} \
	  -device virtio-blk-device,drive=blk \
	  -kernel kernel-edge-faux.elf

check-edge:
	chmod +x scripts/check-edge.sh
	./scripts/check-edge.sh

run-router: kernel-router.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-router.elf

run-router-faux: kernel-router-faux.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-router-faux.elf

check-router:
	chmod +x scripts/check-router.sh
	./scripts/check-router.sh

run-net: kernel-net.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-net.elf

run-net-faux: kernel-net-faux.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-net-faux.elf

check-net:
	chmod +x scripts/check-net.sh
	./scripts/check-net.sh

run-net-prod: kernel-net-prod.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
	  -kernel kernel-net-prod.elf

check-net-prod:
	chmod +x scripts/check-net-prod.sh
	./scripts/check-net-prod.sh

net-prod: check-net-prod

run-llm-net:
	chmod +x scripts/run-llm-net.sh scripts/mk-deepseek-key.sh scripts/mk-deepseek-host.sh tools/deepseek-net-gw.py
	./scripts/run-llm-net.sh

check-llm-net:
	chmod +x scripts/check-llm-net.sh scripts/mk-deepseek-key.sh scripts/mk-deepseek-host.sh tools/deepseek-net-gw.py
	./scripts/check-llm-net.sh

llm-net: check-llm-net

run-ui-console: kernel-ui.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-ui.elf

check-ui-console:
	chmod +x scripts/check-ui-console.sh
	./scripts/check-ui-console.sh

ui-console: check-ui-console

run-ui: kernel-ui.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device \
	  -kernel kernel-ui.elf

check-ui:
	chmod +x scripts/check-ui.sh
	./scripts/check-ui.sh

ui: check-ui

run-console: kernel-console.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console.elf

check-console:
	chmod +x scripts/check-console.sh
	./scripts/check-console.sh

console: check-console

run-console-session: kernel-console-session.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-session.elf

check-console-session:
	chmod +x scripts/check-console-session.sh
	./scripts/check-console-session.sh

console-session: check-console-session

run-console-orch: kernel-console-orch.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-orch.elf

check-console-orch:
	chmod +x scripts/check-console-orch.sh
	./scripts/check-console-orch.sh

console-orch: check-console-orch

run-console-pack: kernel-console-pack.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-pack.elf

check-console-pack:
	chmod +x scripts/check-console-pack.sh
	./scripts/check-console-pack.sh

console-pack: check-console-pack

run-catalog: kernel-catalog.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-catalog.elf

check-catalog:
	chmod +x scripts/check-catalog.sh
	./scripts/check-catalog.sh

catalog: check-catalog

run-console-tenant: kernel-console-tenant.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-tenant.elf

check-console-tenant:
	chmod +x scripts/check-console-tenant.sh
	./scripts/check-console-tenant.sh

console-tenant: check-console-tenant

run-console-audit: kernel-console-audit.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-audit.elf

check-console-audit:
	chmod +x scripts/check-console-audit.sh
	./scripts/check-console-audit.sh

console-audit: check-console-audit

run-console-namespace: kernel-console-namespace.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-namespace.elf

check-console-namespace:
	chmod +x scripts/check-console-namespace.sh
	./scripts/check-console-namespace.sh

console-namespace: check-console-namespace

run-console-quota: kernel-console-quota.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-quota.elf

check-console-quota:
	chmod +x scripts/check-console-quota.sh
	./scripts/check-console-quota.sh

check-console-v7: kernel-console-v7.elf
	chmod +x scripts/check-console-v7.sh
	./scripts/check-console-v7.sh

console-quota: check-console-quota

run-desktop: kernel-desktop.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -device virtio-gpu-device \
	  -device virtio-keyboard-device \
	  -kernel kernel-desktop.elf

check-desktop:
	chmod +x scripts/check-desktop.sh
	./scripts/check-desktop.sh

check-desktop-console:
	chmod +x scripts/check-desktop-console.sh
	./scripts/check-desktop-console.sh

desktop: check-desktop
desktop-console: check-desktop-console

run-box: kernel-box.elf
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -kernel kernel-box.elf

check-box:
	chmod +x scripts/check-box.sh
	./scripts/check-box.sh

box: check-box

bench-ipc:
	chmod +x scripts/bench-ipc.sh
	./scripts/bench-ipc.sh

run-persist2: kernel-persist2.elf
	AGENTOS_DISK=$${AGENTOS_DISK:-/tmp/agentos-persist2.img} \
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -drive if=none,id=blk,format=raw,file=$${AGENTOS_DISK:-/tmp/agentos-persist2.img} \
	  -device virtio-blk-device,drive=blk \
	  -kernel kernel-persist2.elf

check-persist2:
	chmod +x scripts/check-persist2.sh scripts/seed-aos1-disk.py
	./scripts/check-persist2.sh

# --- v4.0 Platform: x86_64-pc ---
PLATFORM ?= riscv64-virt

X86_ARCH_OBJS := kernel/arch/x86/boot.o kernel/arch/x86/trap.o kernel/arch/x86/trap_init.o \
                 kernel/arch/x86/vm.o kernel/arch/x86/timer.o kernel/arch/x86/smp_stub.o \
                 kernel/arch/x86/trap_handler.o kernel/arch/x86/agent_arch.o \
                 kernel/arch/x86/switch.o kernel/arch/x86/virtio_pci_blk.o

X86_WRAP_OBJS := $(X86_ARCH_OBJS) kernel/platform_x86.o kernel/platform_x86_pc.o \
                 kernel/uart_x86.o kernel/printf_x86.o kernel/spinlock_x86.o kernel/halt_x86.o \
                 kernel/mem_x86.o kernel/ipc_x86.o kernel/uaccess_x86.o kernel/ramfs_x86.o \
                 kernel/tool_x86.o kernel/audit_x86.o kernel/namespace_x86.o kernel/quota_x86.o kernel/http-faux_x86.o kernel/agent_x86.o kernel/sched_x86.o \
                 kernel/sched_syscall_x86.o kernel/session_x86.o kernel/persist_x86.o \
                 kernel/llm_stub_x86.o kernel/elfload_stub_x86.o kernel/ota_stub_x86.o \
                 kernel/catalog_stub_x86.o kernel/tenant_stub_x86.o kernel/virtio_ui_stub_x86.o \
                 kernel/kernel_wrap_x86.o user/demo_wrap_x86.o

X86_CONSOLE_SVC_OBJS := user/agent_svc_x86.o user/display_svc_x86.o user/input_svc_x86.o \
                 user/console_svc_x86.o

X86_CONSOLE_KERN_OBJS := $(X86_ARCH_OBJS) kernel/platform_x86.o kernel/platform_x86_pc.o \
                 kernel/uart_x86.o kernel/printf_x86.o kernel/spinlock_x86.o kernel/halt_x86.o \
                 kernel/mem_x86.o kernel/ipc_x86.o kernel/uaccess_x86.o kernel/ramfs_x86.o \
                 kernel/tool_x86.o kernel/audit_x86.o kernel/namespace_x86.o kernel/quota_x86.o \
                 kernel/fleet_stub_x86.o kernel/policy_stub_x86.o kernel/remote_stub_x86.o \
                 kernel/mesh_stub_x86.o \
                 kernel/http-faux_x86.o kernel/agent_x86.o kernel/sched_x86.o \
                 kernel/sched_syscall_x86.o kernel/session_x86.o kernel/tenant_x86.o \
                 kernel/llm_stub_x86.o kernel/elfload_stub_x86.o kernel/persist_stub_x86.o \
                 kernel/ota_stub_x86.o kernel/catalog_stub_x86.o kernel/virtio_ui_stub_x86.o

X86_V01_BETA_KERN_OBJS := $(X86_ARCH_OBJS) kernel/platform_x86_v01.o \
                 kernel/platform_x86_pc.o \
                 kernel/uart_x86.o kernel/printf_x86.o kernel/spinlock_x86.o kernel/halt_x86.o \
                 kernel/mem_x86.o kernel/ipc_x86.o kernel/uaccess_x86.o kernel/ramfs_x86.o \
                 kernel/tool_x86.o kernel/audit_x86.o kernel/namespace_x86.o kernel/quota_x86.o \
                 kernel/fleet_x86.o kernel/policy_x86.o kernel/remote_x86.o kernel/mesh_x86.o \
                 kernel/netstack_stub_x86.o \
                 kernel/http-faux_x86.o kernel/agent_x86.o kernel/sched_x86.o \
                 kernel/sched_syscall_x86.o kernel/session_x86.o kernel/tenant_x86.o \
                 kernel/llm_stub_x86.o kernel/elfload_stub_x86.o kernel/persist_stub_x86.o \
                 kernel/ota_stub_x86.o kernel/catalog_stub_x86.o kernel/virtio_ui_stub_x86.o

X86_V01_BETA_OBJS := $(X86_V01_BETA_KERN_OBJS) kernel/kernel_v01_beta_x86.o \
                 user/demo_v01_beta_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_V02_RC_KERN_OBJS := $(subst kernel/platform_x86_v01.o,kernel/platform_x86_v02.o,$(X86_V01_BETA_KERN_OBJS))
X86_V02_RC_OBJS := $(X86_V02_RC_KERN_OBJS) kernel/kernel_v02_rc_x86.o \
                 user/demo_v02_rc_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_V030_KERN_OBJS := $(subst kernel/platform_x86_v02.o,kernel/platform_x86_v03.o,$(X86_V02_RC_KERN_OBJS))
X86_V030_OBJS := $(X86_V030_KERN_OBJS) kernel/kernel_v030_x86.o \
                 user/demo_v030_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_V040_KERN_OBJS := $(filter-out kernel/netstack_stub_x86.o,$(subst kernel/platform_x86_v03.o,kernel/platform_x86_v04.o,$(subst kernel/http-faux_x86.o,kernel/http_x86.o kernel/netstack_x86.o kernel/arch/x86/virtio_pci_net.o,$(X86_V030_KERN_OBJS))))
X86_V040_OBJS := $(X86_V040_KERN_OBJS) kernel/kernel_v040_x86.o \
                 user/demo_v040_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_CONSOLE_OBJS := $(X86_CONSOLE_KERN_OBJS) kernel/kernel_console_x86_quota.o \
                 user/demo_console_quota_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_CONSOLE_TENANT_OBJS := $(X86_CONSOLE_KERN_OBJS) kernel/kernel_console_x86_tenant.o \
                 user/demo_console_tenant_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_CONSOLE_AUDIT_OBJS := $(X86_CONSOLE_KERN_OBJS) kernel/kernel_console_x86_audit.o \
                 user/demo_console_audit_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_CONSOLE_NAMESPACE_OBJS := $(X86_CONSOLE_KERN_OBJS) kernel/kernel_console_x86_namespace.o \
                 user/demo_console_namespace_x86.o $(X86_CONSOLE_SVC_OBJS)

X86_SMOKE_OBJS := kernel/arch/x86/boot.o kernel/kernel_x86_smoke.o \
                  kernel/platform_x86.o kernel/platform_x86_pc.o \
                  kernel/uart_x86.o kernel/printf_x86.o kernel/spinlock_x86.o \
                  kernel/halt_x86.o

kernel-x86-wrap.elf: $(X86_WRAP_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_WRAP_OBJS)

kernel-x86-console.elf: $(X86_CONSOLE_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_CONSOLE_OBJS)

kernel-x86-console-tenant.elf: $(X86_CONSOLE_TENANT_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_CONSOLE_TENANT_OBJS)

kernel-x86-console-audit.elf: $(X86_CONSOLE_AUDIT_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_CONSOLE_AUDIT_OBJS)

kernel-x86-console-namespace.elf: $(X86_CONSOLE_NAMESPACE_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_CONSOLE_NAMESPACE_OBJS)

kernel-x86-v01-beta.elf: $(X86_V01_BETA_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_V01_BETA_OBJS)

kernel-x86-v02-rc.elf: $(X86_V02_RC_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_V02_RC_OBJS)

kernel-x86-0.3.0.elf: $(X86_V030_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_V030_OBJS)

kernel-x86-0.4.0.elf: $(X86_V040_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_V040_OBJS)

kernel-x86-smoke.elf: $(X86_SMOKE_OBJS) linker_x86.ld
	$(X86_CC) $(X86_LDFLAGS) -o $@ $(X86_SMOKE_OBJS)

kernel/arch/x86/boot.o: kernel/arch/x86/boot.S
	$(X86_CC) -c -o $@ $<

kernel/arch/x86/trap.o: kernel/arch/x86/trap.S
	$(X86_CC) -c -o $@ $<

kernel/%_x86.o: kernel/%.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

user/demo_wrap_x86.o: user/demo_wrap.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o user/demo_wrap_x86.o user/demo_wrap.c

user/demo_console_quota_x86.o: user/demo_console_quota.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_console_quota.c

user/demo_console_tenant_x86.o: user/demo_console_tenant.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_console_tenant.c

user/demo_console_audit_x86.o: user/demo_console_audit.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_console_audit.c

user/demo_console_namespace_x86.o: user/demo_console_namespace.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_console_namespace.c

user/demo_v01_beta_x86.o: user/demo_v01_beta.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_v01_beta.c

user/demo_v02_rc_x86.o: user/demo_v02_rc.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_v02_rc.c

user/demo_v030_x86.o: user/demo_v030.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_v030.c

user/demo_v040_x86.o: user/demo_v040.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/demo_v040.c

user/agent_svc_x86.o: user/agent_svc.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/agent_svc.c

user/display_svc_x86.o: user/display_svc.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/display_svc.c

user/input_svc_x86.o: user/input_svc.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/input_svc.c

user/console_svc_x86.o: user/console_svc.c user/libagent.h include/agentos.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ user/console_svc.c

kernel/http-faux_x86.o: kernel/http.c
	$(X86_CC) $(X86_CFLAGS) -DHTTP_FAUX -c -o $@ $<

kernel/kernel_wrap_x86.o: kernel/kernel_wrap.c
	$(X86_CC) $(X86_CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/kernel_console_x86_quota.o: kernel/kernel_console_x86.c
	$(X86_CC) $(X86_CFLAGS) -DINIT_AGENT=init_console_quota_agent \
		'-DDEMO_TITLE="v6.4 x86 Console quota demo"' \
		'-DCONSOLE_TAG="x86 resource quota"' -c -o $@ $<

kernel/kernel_console_x86_tenant.o: kernel/kernel_console_x86.c
	$(X86_CC) $(X86_CFLAGS) -DINIT_AGENT=init_console_tenant_agent \
		'-DDEMO_TITLE="v6.5 x86 Console tenant demo"' \
		'-DCONSOLE_TAG="x86 multi-tenant"' -c -o $@ $<

kernel/kernel_console_x86_audit.o: kernel/kernel_console_x86.c
	$(X86_CC) $(X86_CFLAGS) -DINIT_AGENT=init_console_audit_agent \
		'-DDEMO_TITLE="v6.5 x86 Console audit demo"' \
		'-DCONSOLE_TAG="x86 audit partition"' -c -o $@ $<

kernel/kernel_console_x86_namespace.o: kernel/kernel_console_x86.c
	$(X86_CC) $(X86_CFLAGS) -DINIT_AGENT=init_console_namespace_agent \
		'-DDEMO_TITLE="v6.5 x86 Console namespace demo"' \
		'-DCONSOLE_TAG="x86 namespace mount"' -c -o $@ $<

kernel/kernel_v01_beta_x86.o: kernel/kernel_v01_beta_x86.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/kernel_v02_rc_x86.o: kernel/kernel_v02_rc_x86.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/kernel_v030_x86.o: kernel/kernel_v030_x86.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/kernel_v040_x86.o: kernel/kernel_v040_x86.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/kernel_v050_riscv.o: kernel/kernel_v050_riscv.c
	$(CC) $(CFLAGS) -c -o $@ $<

user/demo_v050.o: user/demo_v050.c user/libagent.h include/agentos.h
	$(CC) $(CFLAGS) -c -o $@ user/demo_v050.c

kernel/platform_x86_v01.o: kernel/platform.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -DAGENTOS_VERSION=\"0.1.0\" -c -o $@ $<

kernel/platform_x86_v02.o: kernel/platform.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -DAGENTOS_VERSION=\"0.2.0\" -c -o $@ $<

kernel/platform_x86_v03.o: kernel/platform.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -DAGENTOS_VERSION=\"0.3.0\" -c -o $@ $<

kernel/platform_x86_v04.o: kernel/platform.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -DAGENTOS_VERSION=\"0.4.0\" -c -o $@ $<

kernel/http_x86.o: kernel/http.c
	$(X86_CC) $(X86_CFLAGS) -DHTTP_NO_BRIDGE -c -o $@ $<

kernel/arch/x86/virtio_pci_net.o: kernel/arch/x86/virtio_pci_net.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/persist_x86.o: kernel/persist.c
	$(X86_CC) $(X86_CFLAGS) -DENABLE_PERSIST -c -o $@ $<

kernel/llm_stub_x86.o: kernel/llm_stub.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/elfload_stub_x86.o: kernel/elfload_stub.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

check-wrap-x86:
	chmod +x scripts/check-wrap-x86.sh
	./scripts/check-wrap-x86.sh

check-console-x86:
	chmod +x scripts/check-console-x86.sh
	./scripts/check-console-x86.sh

check-console-x86-tenant:
	chmod +x scripts/check-console-x86-tenant.sh
	./scripts/check-console-x86-tenant.sh

check-console-x86-audit:
	chmod +x scripts/check-console-x86-audit.sh
	./scripts/check-console-x86-audit.sh

check-console-x86-namespace:
	chmod +x scripts/check-console-x86-namespace.sh
	./scripts/check-console-x86-namespace.sh

check-console-x86-all:
	chmod +x scripts/check-console-x86.sh scripts/check-console-x86-tenant.sh \
	  scripts/check-console-x86-audit.sh scripts/check-console-x86-namespace.sh
	./scripts/check-console-x86.sh
	./scripts/check-console-x86-tenant.sh
	./scripts/check-console-x86-audit.sh
	./scripts/check-console-x86-namespace.sh

check-v0.1-beta:
	chmod +x scripts/check-v0.1-beta.sh
	./scripts/check-v0.1-beta.sh

check-v0.2-rc:
	chmod +x scripts/check-v0.2-rc.sh
	./scripts/check-v0.2-rc.sh

check-0.1.0: check-v0.1-beta

check-0.2.0: check-v0.2-rc

check-0.3.0:
	chmod +x scripts/check-0.3.0.sh
	./scripts/check-0.3.0.sh

check-0.4.0:
	chmod +x scripts/check-0.4.0.sh
	./scripts/check-0.4.0.sh

check-0.5.0:
	chmod +x scripts/check-0.5.0.sh
	./scripts/check-0.5.0.sh

check-remote-console: check-0.4.0

check-llm-console: check-0.5.0

check-fleet-x86:
	chmod +x scripts/check-fleet-x86.sh
	./scripts/check-fleet-x86.sh

run-x86-v01-beta: kernel-x86-v01-beta.elf
	qemu-system-x86_64 -machine pc -nographic -kernel kernel-x86-v01-beta.elf -display none

run-x86-v02-rc: kernel-x86-v02-rc.elf
	qemu-system-x86_64 -machine pc -nographic -kernel kernel-x86-v02-rc.elf -display none

run-x86-0.3.0: kernel-x86-0.3.0.elf
	qemu-system-x86_64 -machine pc -nographic -kernel kernel-x86-0.3.0.elf -display none

run-x86-0.4.0: kernel-x86-0.4.0.elf
	qemu-system-x86_64 -machine pc -nographic -kernel kernel-x86-0.4.0.elf -display none \
	  -netdev user,id=net0 -device virtio-net-pci,netdev=net0

run-riscv-0.4.0: kernel-console-v040.elf
	$(QEMU) -machine virt -nographic -bios default -global virtio-mmio.force-legacy=false \
	  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
	  -kernel kernel-console-v040.elf

run-riscv-0.5.0: kernel-console-v050.elf
	$(QEMU) -machine virt -nographic -bios default -global virtio-mmio.force-legacy=false \
	  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
	  -kernel kernel-console-v050.elf

run-console-v7: kernel-console-v7.elf
	$(QEMU) -machine virt -nographic -bios default -global virtio-mmio.force-legacy=false \
	  -kernel kernel-console-v7.elf

# --- v4.0 Platform: x86_64-pc smoke (legacy target names) ---
kernel/platform_rv.o: kernel/platform.c include/platform.h
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/platform_x86.o: kernel/platform.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/platform_riscv_virt.o: kernel/platform_riscv_virt.c include/platform.h
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/platform_x86_pc.o: kernel/platform_x86_pc.c include/platform.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/uart_x86.o: kernel/uart.c kernel/uart.h include/platform.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/spinlock_x86.o: kernel/spinlock.c kernel/spinlock.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/printf_x86.o: kernel/printf.c kernel/printf.h kernel/uart.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/halt_x86.o: kernel/halt_x86.c kernel/halt.h
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

kernel/kernel_x86_smoke.o: kernel/kernel_x86_smoke.c
	$(X86_CC) $(X86_CFLAGS) -c -o $@ $<

platform: kernel-x86-smoke.elf

run-x86-smoke: kernel-x86-smoke.elf
	qemu-system-x86_64 -machine pc -nographic -kernel kernel-x86-smoke.elf -display none

check-platform:
	chmod +x scripts/check-platform.sh scripts/check-wrap-x86.sh
	./scripts/check-platform.sh

run-smp: kernel-smp.elf
	$(QEMU) -machine virt -nographic -bios default -smp 2 -kernel kernel-smp.elf

check-smp:
	chmod +x scripts/check-smp.sh
	./scripts/check-smp.sh

run-ota: kernel-ota.elf
	AGENTOS_DISK=$${AGENTOS_DISK:-/tmp/agentos-ota.img} \
	$(QEMU) -machine virt -nographic -bios default \
	  -global virtio-mmio.force-legacy=false \
	  -drive if=none,id=blk,format=raw,file=$${AGENTOS_DISK:-/tmp/agentos-ota.img} \
	  -device virtio-blk-device,drive=blk \
	  -kernel kernel-ota.elf

check-ota:
	chmod +x scripts/check-ota.sh scripts/mk-agentpkg.py
	./scripts/check-ota.sh

run-load: kernel-load.elf
	$(QEMU) -machine virt -nographic -bios default -kernel kernel-load.elf

check-load:
	chmod +x scripts/check-load.sh
	./scripts/check-load.sh

clean:
	rm -f $(COMMON_OBJS) kernel/halt.o kernel/virtio_blk.o kernel/persist.o kernel/persist_stub.o kernel/session.o \
	      kernel/llm_stub.o kernel/llm-faux.o kernel/kernel_v1.o kernel/kernel_memory.o kernel/kernel_memory2.o \
	      kernel/kernel_orchestrator.o kernel/kernel_storage2.o kernel/kernel_tools2.o \
	      kernel/kernel_wrap.o kernel/kernel_vm3.o kernel/kernel_edge.o \
	      kernel/kernel_router.o kernel/kernel_net.o kernel/kernel_load.o \
	      kernel/kernel_persist2.o kernel/kernel_smp.o kernel/kernel_ota.o \
	      kernel/smp.o kernel/ota.o kernel/ota_stub.o \
	      kernel/elfload.o kernel/elfload_stub.o kernel/virtio_net.o kernel/netstack.o kernel/http.o kernel/http-faux.o \
	      kernel-pipeline.o kernel-harness.o kernel-tools.o \
	      kernel-llm.o kernel-llm-faux.o user/demo_pipeline.o user/demo_harness.o \
	      user/demo_tools.o user/demo_llm.o user/demo_v1.o user/demo_memory.o user/demo_memory2.o \
	      user/demo_orchestrator.o user/demo_storage2.o user/demo_tools2.o user/demo_wrap.o \
	      user/demo_vm3.o user/demo_edge.o user/demo_router.o user/router_svc.o \
	      user/demo_net.o user/network_svc.o user/demo_net_prod.o user/network_prod_svc.o \
	      kernel/kernel_net_prod.o user/demo_persist2.o user/demo_smp.o \
	      user/demo_ota.o user/worker_ota_v1.o user/worker_ota_v2.o \
	      user/worker_ota_v1_elf.inc user/worker_ota_v2_pkg.inc \
	      worker-ota-v1.agent worker-ota-v2.agent worker-ota-v2.agentpkg \
	      user/worker_load.o user/demo_load.o user/worker_elf.inc worker.agent \
	      kernel.elf kernel-harness.elf kernel-tools.elf kernel-llm.elf \
	      kernel-llm-faux.elf kernel-v1.elf kernel-memory.elf kernel-memory-faux.elf \
	      kernel-memory2.elf kernel-memory2-faux.elf \
	      kernel-orchestrator.elf kernel-orchestrator-faux.elf kernel-storage2.elf \
	      kernel-tools2.elf kernel-wrap.elf kernel-vm3.elf \
	      kernel-edge.elf kernel-edge-faux.elf \
	      kernel-router.elf kernel-router-faux.elf \
	      kernel-net.elf kernel-net-faux.elf kernel-net-prod.elf kernel-llm-net.elf kernel-ui.elf \
	      kernel-console.elf kernel-console-session.elf kernel-console-orch.elf kernel-console-pack.elf kernel-catalog.elf \
	      kernel-desktop.elf \
	      kernel/tls_client.o kernel/llm_deepseek.o kernel/mbedtls_port.o kernel/string.o \
	      kernel/llm_net.o kernel/kernel_llm_net.o kernel/virtio_gpu.o kernel/virtio_input.o \
	      kernel/virtio_ui_stub.o kernel/kernel_ui.o user/demo_ui.o user/display_svc.o user/input_svc.o \
	      kernel/kernel_console.o user/demo_console.o \
	      kernel/kernel_console_session.o user/demo_console_session.o \
	      kernel/kernel_console_orch.o user/demo_console_orch.o user/orch_pipeline.o user/storage_svc.o \
	      kernel/kernel_console_pack.o user/demo_console_pack.o \
	      user/console_svc.o \
	      generated/deepseek_key.h generated/deepseek_host.h \
	      kernel-smp.elf \
	      kernel-ota.elf kernel-ota-old.elf \
	      kernel-load.elf \
	      kernel-x86-smoke.elf \
	      kernel-persist2.elf \
	      kernel/platform_rv.o kernel/platform_x86.o \
	      kernel/platform_riscv_virt.o kernel/platform_x86_pc.o \
	      kernel/arch/x86/boot.o kernel/uart_x86.o kernel/printf_x86.o \
	      kernel/halt_x86.o kernel/kernel_x86_smoke.o
