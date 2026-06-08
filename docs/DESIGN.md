# AgentOS 设计规格

日期：2026-06-08

## 目标

在 **RISC-V (QEMU virt)** 与 **x86 (QEMU pc)** 参考机上实现以 Agent 为一等公民的操作系统原型，验证：

- Agent 生命周期与调度
- 消息 IPC 与 Capability 权限
- Tool 网关
- 多 Agent 协作 pipeline
- U-mode 隔离与 VirtIO LLM 接入
- **块设备持久化与 session 日志**

## 架构

见 [ARCHITECTURE.md](./ARCHITECTURE.md)。**未来方向与分阶段计划**见 [ROADMAP.md](./ROADMAP.md)。

## 核心接口

| 组件 | 文件 | 职责 |
|------|------|------|
| Syscall ABI | `include/agentos.h` | 编号、结构体、`ecall` 封装 |
| 内核 | `kernel/*.c` | 调度、IPC、VM、VirtIO、Agent 管理 |
| 持久化 | `kernel/persist.c`, `kernel/virtio_blk.c` | AOS1/AOS2 ramfs ↔ VirtIO block；在线迁移 |
| Session | `kernel/session.c` | 按 Agent 追加/读取 session 日志 |
| libagent | `user/libagent.h` | Agent 应用封装 |
| Demo | `user/demo_pipeline.c` | init → planner ↔ worker |
| LLM Demo | `user/demo_llm.c` | init → llm-worker → DeepSeek |
| v1 Demo | `user/demo_v1.c` | init + storage(2) + worker：IPC FS + session + persist |

## 调度模型

- **协作式**：`agent_yield()` / 阻塞 recv 循环中 yield
- **抢占式**：S-mode timer 中断；仅在 yield/exit 后启用 STIE，返回 U-mode 后由 `sret` 打开 SIE
- **上下文**：`struct trapframe` 保存通用寄存器 + `sepc`/`sstatus`
- **切换**：`switch_to_agent()` / trap 返回路径 `sret` 至 U-mode
- **IPC 优先**：`pick_next()` 优先调度 inbox 非空的 Agent
- **公平 RR**：RUNNABLE Agent 按 `rr_next` 轮询，避免低 id 饥饿

## 可靠性 (v1.1)

- **IPC 边界**：`agent_send`/`agent_recv` 经 `copy_from_user` / `copy_to_user`
- **页故障**：U-mode 页故障杀死 faulting Agent 并继续调度，不拖垮全局
- **干净退出**：无 runnable Agent 时 SBI SRST shutdown（QEMU 正常退出）

## 内存模型 (v0.3+ / v3.0)

- SV39 页表；内核映射无 `PTE_U`，用户代码/栈带 `PTE_U`
- 每 Agent 浅拷贝根页表（`vm_create_agent_pt`），独立用户栈映射
- 用户代码链接至 `0x80300000`；Agent 栈 VA 自 `0x40000000` 起，按 `USER_STACK_SLOT`（栈 + guard）偏移
- v3.0：demand heap、`copy_*_user` EFAULT、栈 guard 页、用户段 W^X（见 [SYSCALL.md](./SYSCALL.md)）

## 持久化 (v0.7 / v1.0)

- **块设备**：QEMU `-device virtio-blk-device` + MMIO v2（`virtio-mmio.force-legacy=false`）
- **格式 AOS1/AOS2**：superblock（magic + version + file_count + checksum）+ 每文件 meta + 数据扇区
- **AOS2**：meta 增加 `crc32`；`persist_sync()` 始终写 AOS2；见 AOS1 时自动 migrate
- **范围**：除 `/sys/*` 外的 ramfs 用户文件（含 `/agent/*`、`/session/*`、`/audit/*`）
- **API**：`SYS_AGENT_SYNC` → `persist_sync()`；启动时 `persist_load()`

## Storage 服务 (v0.8)

- 内核注册 storage agent（id=2，`CAP_SVC_STORAGE | CAP_FS`）
- Worker 经 `agent_svc_read/write` 发 `MSG_FS_*` IPC，payload 写请求用 `path|data`
- storage 收到后复制消息字段再调 Tool，避免 syscall 破坏栈上 `msg`

## Session (v1.0)

- 路径：`/session/agent/<id>/log`
- Tool：`TOOL_SESSION_APPEND` / `TOOL_SESSION_TAIL`（需 `CAP_LOG`）
- 语义：append 追加一行；tail 返回最后一行（不含 `\n`）

## 验证

```bash
make run          # pipeline demo
make run-harness  # steer / compact demo
make run-tools    # ramfs + system tools demo
make run-llm      # DeepSeek LLM demo（需 .env）
make run-llm-faux # 无网络的 LLM 自测
make run-v1       # VirtIO persist + session demo
```

v1 预期：`[init] v1 demo complete` 后 halting；二次启动同一磁盘应出现 `[persist] loading N files from AOS2 block`（AOS1 盘首次会先 migrate）。

## 版本历史

| 版本 | 内容 | 状态 |
|------|------|------|
| v0.1 | S-mode 直接 syscall、协作调度、IPC、Tool | ✅ |
| v0.2 | U-mode + `ecall` + timer 抢占框架 | ✅ |
| v0.3 | 每 Agent SV39 页表隔离 | ✅ |
| v0.4 | VirtIO console + DeepSeek bridge + `agent_llm` | ✅ |
| v0.5 | Pi-inspired IPC（steer/follow-up/compact）+ LLM 流式 + faux | ✅ |
| v0.6 | 统一 Tool 网关 + ramfs + TIME/READ/WRITE + policy | ✅ |
| v0.7 | VirtIO block + AOS1 ramfs 持久化 | ✅ |
| v0.8 | storage 服务 Agent + IPC 文件访问 | ✅ |
| v1.0 | session + `agent_sync` + demo_v1 + 文档 | ✅ |
| v1.1 | IPC copy 边界 + 页故障隔离 + RR 调度 + SBI shutdown | ✅ |
| v3.0 | demand heap + EFAULT + COW + stack guard + W^X | ✅ |
| v3.1 | edge sensor pipeline + check-edge | ✅ |
| v3.2 | model-router service + agent_svc_llm + check-router | ✅ |
| v3.3 | agent ELF load + `SYS_AGENT_LOAD` + check-load | ✅ |
| v3.4 | network service + virtio_net + agent_svc_http + check-net | ✅ |
| v3.5 | Persist AOS2 + migrate + check-persist2 | ✅ |
| v4.0–v4.4 | Platform HAL / SMP / OTA / Net / UI | ✅ |
| v5.0–v5.6 | Console 产品线 + Desktop + Catalog | ✅ |
| v6.0–v6.5 | Multi-tenant + x86 Console 矩阵 | ✅ |

## 后续

详见 [ROADMAP.md](./ROADMAP.md) §3B（**v7 Field Pilot**）与 [V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md)（生产化路径）。

**当前基线**：**v6.5**（多租户 + 双平台 Console 验收全绿）。**下一迭代**：**v7.0 Fleet 遥测**。  
**GA 1.0 目标**：**2027 年 Q3**（字段试点 v7 可于 2026 Q4 启动 POC）。
