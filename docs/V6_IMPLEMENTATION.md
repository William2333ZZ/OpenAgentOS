# v6.x Multi-tenant & Audit — 实现设计

---

## v6.0 Multi-tenant ✅

状态：MVP 完成（session quota + 隔离 probe）  
依赖：v5.6 Package Catalog ✅、v5.1 Session ✅

### 目标

按 **Agent** 隔离 session 与 quota，为 audit 分区与后续硬隔离打基础。

| 能力 | MVP |
|------|-----|
| Session 配额 | 每 Agent 640 字节（`TENANT_SESSION_QUOTA`） |
| 跨 Agent session 读 | `TOOL_READ` / probe 返回 EPERM |
| Console 命令 | `/tenant status` `/tenant probe <id>` |
| 验收 | `make check-console-tenant` |

### 内核组件

| 文件 | 职责 |
|------|------|
| `include/tenant.h` | 配额常量 + API |
| `kernel/tenant.c` | 用量统计、quota 检查、probe |
| `kernel/tenant_stub.c` | 非 tenant 内核 no-op |
| `kernel/session.c` | append 前调用 `tenant_session_allow` |
| `kernel/tool.c` | `TOOL_TENANT` + session 路径 FS 策略 |

### TOOL_TENANT (18)

| cmd | 行为 |
|-----|------|
| `TENANT_CMD_STATUS` (0) | 打印 used/quota |
| `TENANT_CMD_PROBE` (1) | 尝试跨 Agent session 读，期望 EPERM |

### Console

```
/tenant status          # 当前 Agent session 用量
/tenant probe 1         # console(id=10) 探测 agent 1 → isolation ok
/quit tenant
```

### 产物

```
kernel/kernel_console_tenant.c
user/demo_console_tenant.c
kernel-console-tenant.elf
scripts/check-console-tenant.sh
```

---

## v6.1 Audit 分区 ✅

状态：MVP 完成（tool.log 配额 + compact + 跨 Agent 读隔离）  
依赖：v6.0 tenant ✅、v2.2 tool audit ✅

### 目标

为每个 Agent 提供独立 **audit 分区**，Tool Gateway 自动 append 调用记录；Console 提供运维入口。

| 能力 | MVP |
|------|-----|
| Audit 路径 | `/audit/agent/<id>/tool.log` |
| 配额 | 512 字节（`AUDIT_QUOTA_BYTES`） |
| 满配额 | `tool_audit_log` 触发 compact(8) 后重试 |
| 跨 Agent audit 读 | `TOOL_READ` / probe → EPERM |
| Console | `/audit status\|tail\|compact [n]\|probe <id>` |
| 热路径 | `TOOL_CONSOLE` READ_CHAR/PUTS/PROMPT 不写 audit |
| 验收 | `make check-console-audit` |

### 内核组件

| 文件 | 职责 |
|------|------|
| `include/audit.h` | 配额 + API |
| `kernel/audit.c` | path、usage、append、compact、tail、probe |
| `kernel/audit_stub.c` | 非 audit 演示内核 no-op |
| `kernel/tool.c` | `TOOL_AUDIT`；`tool_audit_log` → `audit_write`；FS 策略 |

### TOOL_AUDIT (19)

| cmd | 行为 |
|-----|------|
| `AUDIT_CMD_STATUS` (0) | 打印 used/quota，返回 used |
| `AUDIT_CMD_TAIL` (1) | arg1=max_bytes，打印尾部 |
| `AUDIT_CMD_PROBE` (2) | arg1=目标 id，跨 Agent 读 probe |
| `AUDIT_CMD_COMPACT` (3) | arg1=保留行数（1–32），写入 `compact: N` 头 |

### Console

```
/audit status           # audit 用量
/audit tail             # 尾部 128 字节
/audit compact 4        # 保留最近 4 行
/audit probe 1          # 跨 Agent 读 → isolation ok
/quit audit
```

### 产物

```
kernel/kernel_console_audit.c
user/demo_console_audit.c
kernel-console-audit.elf
scripts/check-console-audit.sh
```

linker.ld 需将 `demo_console_audit.o` 放入 `.user.text`（与 tenant demo 相同）。

### 验收 ✅

`make check-console-audit`（~4s）：session append 灌满 tool.log → status → compact/tail → probe EPERM → complete。

`make check-wrap` 回归：持久化路径与 user `build_audit_path` 一致（`/audit/agent/<id>/tool.log`）。

---

## v6.2 Namespace mount ✅

状态：MVP 完成（每 Agent 独立 home mount + 跨 namespace 读隔离）  
依赖：v6.1 audit ✅

### 目标

在 session/audit 路径隔离之上，引入统一的 **mount 视图**：每 Agent 私有 home 目录，Tool Gateway FS 策略经 `namespace_path_ok` 裁决。

| 能力 | MVP |
|------|-----|
| Mount 根 | `/ns/agent/<id>/home/` |
| 私有文件 | `secret`（256B 配额 `NAMESPACE_HOME_QUOTA`） |
| 跨 Agent 读 | `TOOL_READ` / probe → EPERM |
| Console | `/namespace status\|write <text>\|probe <id>` |
| 验收 | `make check-console-namespace` |

### 内核组件

| 文件 | 职责 |
|------|------|
| `include/namespace.h` | mount 路径 + API |
| `kernel/namespace.c` | path、quota、write、probe |
| `kernel/namespace_stub.c` | 非 demo 内核 no-op |
| `kernel/tool.c` | `TOOL_NAMESPACE`；FS 策略接入 `/ns/` |

### TOOL_NAMESPACE (20)

| cmd | 行为 |
|-----|------|
| `NS_CMD_STATUS` (0) | 打印 root/used/quota |
| `NS_CMD_PROBE` (1) | 跨 Agent 读 home/secret，期望 EPERM |
| `NS_CMD_WRITE` (2) | arg1=文本，写入本 Agent home/secret |

### Console

```
/namespace status
/namespace write console-ns-secret
/namespace probe 1          # → isolation ok
/quit namespace
```

### 产物

```
kernel/kernel_console_namespace.c
user/demo_console_namespace.c
kernel-console-namespace.elf
scripts/check-console-namespace.sh
```

### 验收 ✅

`make check-console-namespace`（~3s）：write → status → probe EPERM → complete。

---

## v6.3 Resource quota ✅

状态：MVP 完成（IPC / Tool / FS 三维 cgroup 等价物）  
依赖：v6.2 namespace ✅

### 目标

在路径隔离之上，为每个 Agent 施加 **可观测、可验收** 的资源上限。

| 维度 | 配额 |  enforcement |
|------|------|--------------|
| IPC send | 16 (`QUOTA_IPC_MAX`) | `agent_send` / `agent_send_kernel` |
| Tool 调用 | 128 (`QUOTA_TOOL_MAX`) | `tool_dispatch`（Console I/O 热路径除外） |
| FS 文件 | 32 (`QUOTA_FS_FILES`) | `/agent/<id>/` 新文件创建 |

| Console | `/quota status\|burn <n>\|probe` |
| 验收 | `make check-console-quota` |

### TOOL_QUOTA (21)

| cmd | 行为 |
|-----|------|
| `QUOTA_CMD_STATUS` (0) | 打印 ipc/tool/fs used/max |
| `QUOTA_CMD_BURN` (1) | arg1=N，消耗 IPC 配额（发往 display） |
| `QUOTA_CMD_PROBE` (2) | 检测 IPC 是否仍可发送（不实际发送） |
| `QUOTA_CMD_NOTIFY` (3) | 配额满时 demo 退出通知（不计入 IPC） |

### 产物

```
kernel/kernel_console_quota.c
user/demo_console_quota.c
kernel-console-quota.elf
scripts/check-console-quota.sh
```

### 验收 ✅

`make check-console-quota`（~3s）：burn 16 → status 16/16 → probe ENOSPC → quit complete。

---

## v6.4 x86 Console 子集 ✅

### 目标

在 **x86_64-pc** 上运行 Console REPL 子集（复用 v6.3 quota demo），验证 trap/VM/Agent 用户态与 RISC-V 行为一致。

### 设计

| 项 | 决策 |
|----|------|
| Demo | `/quota status|burn|probe` + `/quit quota`（与 v6.3 同脚本路径） |
| 内核 | `kernel/kernel_console_x86.c`：tenant/audit/namespace/quota + display/input/console 服务 |
| 用户态 | `demo_console_quota.c` + `agent_svc`/`display_svc`/`input_svc`/`console_svc` 编译为 `*_x86.o` |
| 链接 | `linker_x86.ld`：`*_svc_x86.o` 与 `demo_console*.o` 必须进 `.user.*`（否则 rodata 落内核段 → NOTIFY EFAULT） |
| UI | `virtio_ui_stub_x86.o`；display/input/console 走 UART 回退 |

### 产物

```
kernel/kernel_console_x86.c
kernel-x86-console.elf
scripts/check-console-x86.sh
```

### 验收 ✅

`make check-console-x86`（~4s）：与 `check-console-quota` 同断言（ipc burn + probe ENOSPC + complete）。

---

## v6.5 x86 Console 扩展 ✅

### 目标

将 v6.0–v6.2 的 Console demo（tenant / audit / namespace）移植到 x86，与 v6.4 quota 子集组成完整 x86 Console 验收矩阵。

### 设计

| 项 | 决策 |
|----|------|
| 构建 | 共享 `X86_CONSOLE_KERN_OBJS` + 按 demo 链接 `kernel_console_x86_{quota,tenant,audit,namespace}.o` |
| 宏 | `-DINIT_AGENT` / `-DDEMO_TITLE` / `-DCONSOLE_TAG` 复用 `kernel_console_x86.c` |
| 验收 | `check-console-x86-{tenant,audit,namespace}` + `check-console-x86-all` |
| x86 修复 | `audit_compact` 改用 static `line_starts[256]`，避免 x86 内核栈溢出（32KB VLA） |

### 产物

```
kernel-x86-console-tenant.elf
kernel-x86-console-audit.elf
kernel-x86-console-namespace.elf
scripts/check-console-x86-{tenant,audit,namespace}.sh
scripts/check-console-x86-all.sh (via Makefile)
```

### 验收 ✅

`make check-console-x86-all`（~17s）：quota + tenant + audit + namespace 四条 x86 路径全绿。

---

## v6.6+ 路线

v7 详细设计见 **[V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md)**（Fleet / Remote Console / Policy / 生产化时间表）。

原则：**不换 Agent ABI / Tool 编号**；Console 为产品入口；每子版本一个 `make check-*`。
