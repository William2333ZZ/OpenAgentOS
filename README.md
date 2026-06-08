# OpenAgentOS

[![CI](https://github.com/William2333ZZ/OpenAgentOS/actions/workflows/ci.yml/badge.svg)](https://github.com/William2333ZZ/OpenAgentOS/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

以 **Agent 为一等公民** 的轻量操作系统：**OpenAgentOS**。Capability 隔离、服务化 Agent（storage / router / network）、AOS2 持久化、动态 ELF 加载。当前默认运行在 QEMU **RISC-V `virt`** 与 **x86 `pc`** Console 子集。

**产品路线图**：[docs/PRODUCT_ROADMAP.md](docs/PRODUCT_ROADMAP.md) · **个人品牌**：[docs/BRAND.md](docs/BRAND.md)

Agent IPC 设计参考 [pi Agent Harness](https://github.com/earendil-works/pi)（MIT，未捆绑在本仓库）；系统能力经统一 Tool 网关暴露。

## 特性

- Agent 生命周期管理（create / exit）
- U-mode Agent + `ecall`  syscall
- SV39 页表：每 Agent 独立用户栈映射
- 协作式 + timer 抢占调度
- 消息 IPC（环形邮箱，16 条/Agent）
- Capability 权限控制
- 统一 Tool 网关（`kernel/tool.c`）：LOG / ADD / TIME / READ / WRITE / LLM / SESSION + policy
- ramfs 内存文件系统：`/sys/*` 只读，`/agent/<id>/*` 私有读写
- **VirtIO block 持久化**：ramfs 用户文件序列化到块设备（**AOS2**，兼容 AOS1 自动迁移），跨重启恢复
- **v2.3 v2.x 收尾**：ramfs64、uaccess 边界、audit 跨重启 persist
- **v3.0 内存子系统**：demand heap、用户指针 EFAULT 校验、页故障隔离、heap COW、用户代码 W^X、栈 guard 页
- Pi-inspired Agent IPC：steer 高优先级队列、follow-up、inbox compaction
- Agent phase 跟踪（turn / tool / llm / wait-ipc）
- VirtIO JSONL 流式 LLM + faux 自测模式
- Demo：init → planner ↔ worker；init → llm-worker；**v1 persist + session demo**

## 前置依赖

```bash
# macOS
brew install riscv64-elf-gcc qemu
# v4.0 x86 smoke 可选：
brew install i686-elf-gcc python3
```

## 构建与运行

```bash
make
make run          # pipeline demo
make run-harness  # steer / compact demo
make run-tools    # ramfs + system tools demo
make run-v1       # VirtIO persist + session demo（需块设备镜像）
make run-memory-faux  # v1.2 memory+LLM（离线 faux，无需 API key）
make run-memory   # v1.2 memory+LLM + DeepSeek bridge
make check-memory # v1.2 双启动验收（llm + sync + 跨 boot session）
make run-orchestrator-faux  # v1.3 五 Agent 编排（离线）
make check-orchestrator   # v1.3 自动化验收
make run-memory2-faux     # v2.0 session compact + LLM stream
make check-memory2        # v2.0 双启动验收
make run-storage2         # v2.1 并发 FS + req_id RPC
make check-storage2       # v2.1 双 worker 交错验收
make run-tools2           # v2.2 Tool audit + gpio/http stub
make check-tools2         # v2.2 policy EPERM + audit 验收
make run-wrap             # v2.3 v2.x 收尾（ramfs64 + audit 落盘）
make check-wrap           # v2.3 双启动验收
make run-vm3              # v3.0 demand heap + EFAULT demo
make check-vm3            # v3.0 自动化验收
make run-edge-faux        # v3.1 边缘传感 demo（faux LLM + persist）
make check-edge           # v3.1 双启动验收
make run-router-faux      # v3.2 model-router demo
make check-router         # v3.2 faux + offline 验收
make run-load             # v3.3 agent ELF load demo
make check-load           # v3.3 双启动：ELF 落盘 + agent_load + worker sum=42
make run-net-faux         # v3.4 network service demo
make check-net            # v3.4 faux + offline + audit 验收
make run-persist2         # v3.5 AOS1→AOS2 迁移 demo
make check-persist2       # v3.5 三阶段：seed AOS1 → migrate → AOS2 reload
make check-platform       # v4.0 RISC-V 回归 + x86 wrap
make check-smp            # v4.1 双 hart SMP demo
make check-ota            # v4.2 OTA Agent 包升级
make check-net-prod       # v4.3 VirtIO-net 原生 HTTP + cache + quota
make run-x86-smoke        # 仅 x86 Multiboot smoke（需 i686-elf-gcc）
# 或
./scripts/run-qemu.sh
./scripts/run-v1.sh
```

### v1.0 持久化 + Storage IPC Demo

```bash
make run-v1
# 二次启动验证跨重启恢复（使用同一镜像）：
AGENTOS_DISK=/tmp/agentos-v1.img make run-v1
make check-v1   # 自动化验证（约 2s，无需手动 Ctrl+C）
```

三 Agent 架构：**init(1) + storage(2) + worker(3)**。Worker 无 `CAP_FS`，文件读写经 `MSG_FS_*` IPC 委托 storage。

预期输出末尾：

```
[worker] persist sync ok
[worker] session tail: turn1:hello-v1-persist
[init] v1 demo complete
[kernel] no runnable agents, halting
```

二次启动时应先看到 `[persist] loading N files from AOS2 block`（旧 AOS1 盘首次启动会先 migrate）。

### v1.2 Memory + LLM 联合 Demo

一条链路打通 **storage IPC → session → agent_llm → persist sync**，二次启动从块设备恢复 session 历史。

```bash
make run-memory-faux          # 离线（LLM_FAUX 回答 42，无需 .env）
make check-memory             # 双启动自动化验收
make run-memory               # 真 DeepSeek（需 DEEPSEEK_API_KEY）
```

架构同 v1.0（init=1, storage=2, memory-worker=3），worker 额外持有 `CAP_LLM`。

预期 boot1 末尾：

```
[memory] llm answer: 42
[memory] persist sync ok
[init] memory demo complete
```

预期 boot2 应先看到 `[persist] loading ...` 与 `[memory] restored prior session tail: assistant:42`。

### v1.3 Orchestrator 五 Agent 编排

init(1) + storage(2) + planner(3) → worker(4) + llm-worker(5) 协作流水线。

```bash
make run-orchestrator-faux   # 离线 LLM_FAUX
make check-orchestrator      # 30s 内验收五 Agent pipeline
make run-orchestrator        # 真 DeepSeek
```

流程：planner 派 TASK → worker `ADD` + `agent_svc_read` → planner 收结果 → llm-worker `agent_llm` 总结 → shutdown。

### v2.0 Session 增强 + LLM 流式

长对话 **compact**、**MSG_SUMMARY** 入 inbox、LLM delta 自动写入 session。

```bash
make run-memory2-faux     # 离线（steer + compact + LLM stream）
make check-memory2        # compact + summary + delta + 跨 boot persist
```

新增 Tool：`TOOL_SESSION_READ`（读最近 N 行）、`TOOL_SESSION_COMPACT`（保留 N 行 + 推送 `MSG_SUMMARY`）。内核在 `agent_llm` 完成后自动 `session_append("assistant: ...")`。

### v2.1 Storage v2 — 并发 FS + req_id

两个 worker 经 `agent_svc_read/write` 并发访问 storage，请求带 `R<id>|path` / `W<id>|path|data`，响应严格匹配 `req_id`，避免交错 RPC 错乱。

```bash
make run-storage2       # init + storage + worker-a/b
make check-storage2     # 自动化验收（readback ok + workerA/B-data）
```

架构：**init(1) + storage(2) + worker-a(4) + worker-b(5)**。storage 仍支持 legacy 无 prefix 请求（`parse_req_id` 失败时 body=req）。

### v2.2 Tool 扩展与 audit

可插拔 `tool_table` 新增 `TOOL_GPIO` / `TOOL_HTTP` stub；每次 Tool 调用写入 `/audit/agent/<id>/tool.log`（含 EPERM 拒绝）。

```bash
make run-tools2       # probe + allowed worker
make check-tools2     # EPERM + audit 条目 + gpio/http stub
```

新增 CAP：`CAP_GPIO`、`CAP_NET`。无 CAP 调用返回 EPERM 且 audit 记录 `tool=N rc=-1`。

### v2.3 v2.x 收尾

2.x 技术债收束：**ramfs 64 文件**、**agent_write/create 经 copy_from_user**、**trap_handler 去除栈上 local 返回**、**audit 日志跨重启 persist**。

```bash
make run-wrap       # boot1: 20 文件 stress + gpio EPERM + sync
make check-wrap     # boot2: 恢复 stress + audit 验证
```

Pipeline demo 预期输出末尾：

```
[init] all agents finished: pipeline-complete
[kernel] no runnable agents, halting
```

## LLM Demo（DeepSeek）

Guest 内 Agent 经 VirtIO console 连宿主机 bridge，调用 DeepSeek API。

```bash
cp .env.example .env   # 填入 DEEPSEEK_API_KEY
make run-llm
```

### LLM Native Network（v4.3.1）

Guest 内 VirtIO-net + mbedTLS 调用 DeepSeek，不依赖 `tools/deepseek-bridge.py`（VirtIO console）。

```bash
cp .env.example .env   # DEEPSEEK_API_KEY
make run-llm-net       # 自动启动 host gateway + QEMU
```

CI / 验收（需网络与 API key）：

```bash
make check-llm-net
# 或
make llm-net
```

架构：Guest DHCP → DNS/DoH → HTTP(S) POST；若 CDN TLS 握手失败，回退至宿主机 `tools/deepseek-net-gw.py`（`10.0.2.2:8443`）。

### Human Interface（v4.4）

display/input 服务 Agent；**GPU 可选**——无 VirtIO-GPU 时自动走 UART 控制台（适合无显卡 PC、SSH、服务器）：

```bash
make run-ui              # 有 GPU：加 -device virtio-gpu-device；无则纯串口
make run-ui-console      # 故意不挂 GPU，串口交互
make check-ui            # CI：virtio-gpu + 键盘注入
make check-ui-console    # CI：无 GPU，stdin 送 'h'
```

### Console REPL（v5.0）

无 GPU 单行 REPL：`/help` `/echo` `/llm` `/agents` `/quit`；console-agent(id=10) + router(6) + display(8)：

```bash
make run-console         # 无 GPU，UART REPL
make check-console       # CI：fifo 注入 /help、/llm、/quit
make check-console-session  # CI：session append/tail/compact
```

### Console Session（v5.1）

在 v5.0 REPL 上增加 pi 语义：`/session` `/compact` `/steer`：

```bash
make run-console-session
make check-console-session
```

### Console Orchestrator（v5.2）

Console 触发 v1.3 编排链（planner → worker → llm-worker）：

```bash
make run-console-orch
make check-console-orch     # CI：/run orch + sum=42（~5s）
```

### Console Pack Launcher（v5.3）

Console 内动态加载 `.agent` 包：`/list` `/load` `/ota`；init 处理 `MSG_CONSOLE_REQ`：

```bash
make run-console-pack
make check-console-pack    # CI：list + load worker.agent + sum=42（~4s）
```

### Desktop Shell（v5.4）

GPU 全屏文本 launcher；按 `1` 加载 `worker.agent`。无 GPU 时 init 提示回退 Console：

```bash
make run-desktop             # 需 -device virtio-gpu-device + virtio-keyboard-device
make check-desktop           # CI：GPU + sendkey 1 → sum=42（~5s）
make check-desktop-console   # 无 GPU → init fallback
```

### Headless Box（v5.5）

无 display/input 服务，纯 UART Console 管理 + IPC bench：

```bash
make run-box                 # headless UART REPL
make check-box               # CI：list + load + quit box（~3s）
make bench-ipc               # M1：32 轮 ping-pong tick 计数
```

### Package Catalog（v5.6）

签名包目录 + 版本约束 + 回滚；Console `/catalog list|install|rollback`：

```bash
make run-catalog
make check-catalog           # CI：list + install v2 + rollback v1（~15s）
```

### Multi-tenant（v6.0）

Session 配额 + 跨 Agent 隔离 probe：

```bash
make run-console-tenant
make check-console-tenant  # CI：quota + probe EPERM（~15s）
```

### Audit 分区（v6.1）

每 Agent `/audit/agent/<id>/tool.log` 512B 配额 + compact 轮转 + 跨 Agent 读隔离：

```bash
make run-console-audit
make check-console-audit   # CI：quota + compact + probe EPERM（~4s）
```

### Namespace mount（v6.2）

每 Agent `/ns/agent/<id>/home/` 私有 mount + 跨 namespace probe：

```bash
make run-console-namespace
make check-console-namespace  # CI：write + probe EPERM（~3s）
```

### Resource quota（v6.3）

每 Agent IPC/Tool/FS 三维配额：

```bash
make run-console-quota
make check-console-quota    # CI：ipc burn + probe ENOSPC（~3s）
```

### OpenAgentOS 0.3.0（当前）

Fleet HTTP ingest + Remote ping（HTTP_FAUX 链路）：

```bash
make run-x86-0.3.0
make check-0.3.0          # fleet push → ingest + remote ping (~6s)
```

详见 [docs/VERSIONING.md](docs/VERSIONING.md)。

### OpenAgentOS 0.2.0（v0.2-rc）

双平台 v7 + Policy 文件加载：

```bash
make run-x86-v02-rc
make check-0.2.0          # 别名 check-v0.2-rc (~4s)
make check-console-v7     # RISC-V v7 栈 (~5s)
make check-fleet-x86     # x86 Fleet 子集
```

详见 [docs/V0.2_RC.md](docs/V0.2_RC.md)。

### OpenAgentOS 0.1.0（v0.1-beta GA）

在当前 PC 上运行完整 Field Pilot 栈（v6 + v7 合并）：

```bash
make run-x86-v01-beta       # 交互
make check-0.1.0              # 别名 check-v0.1-beta (~4s)
```

详见 [docs/V0.1_BETA.md](docs/V0.1_BETA.md)。

### x86 Console 子集（v6.4 / v6.5）

在 `x86_64-pc` 上跑 Console REPL 子集（UART 回退，无 GPU）：

```bash
make check-console-x86              # v6.3 quota（~4s）
make check-console-x86-tenant       # v6.0 tenant（~8s）
make check-console-x86-audit        # v6.1 audit（~12s）
make check-console-x86-namespace    # v6.2 namespace（~4s）
make check-console-x86-all          # 以上全部
```

### Console 验收与耗时

四条 Console 验收均通过 `scripts/check-console-common.sh`：**通过 ~4–5s/项**；失败 **~30s 内退出**（检测 page fault / 超时）。详见 [docs/OS_BENCHMARK.md](docs/OS_BENCHMARK.md)。

```bash
make check-console check-console-session check-console-orch check-console-pack check-catalog check-console-tenant check-console-audit check-console-namespace check-console-quota check-console-x86-all
```

### Horizon 2 — v0.1-beta GA（已实现）

**v0.1-beta** 将 v7.0–v7.3 与 v6 Console 产品线合并为可在 **x86_64-pc QEMU** 上运行的子 OS。长期 **GA 1.0** 仍为 2027 Q3 目标。详见 [docs/V0.1_BETA.md](docs/V0.1_BETA.md) · [docs/V7_IMPLEMENTATION.md](docs/V7_IMPLEMENTATION.md)。

| 阶段 | 版本 | 目标时间 | 说明 |
|------|------|----------|------|
| 演示/研究 | v1–v6.5 | **2026-06 ✅** | 全量 `check-*` + 双平台 Console |
| **字段试点 GA** | **v0.1-beta** | **2026-06 ✅** | x86 Fleet/Remote/Policy/Mesh + `check-v0.1-beta` |
| 生产候选 | v8.x | **2027 Q2** | 真板 BSP、安全硬化 |
| **GA 1.0** | v9 | **2027 Q3** | stable LTS、集成商 SLA |

v0.1-beta 验收：

```bash
make check-v0.1-beta
```

无网络 CI 自测：

```bash
make run-llm-faux
```

正常约 **5–15 秒**内应看到 `[llm-worker] answer: 42` 并 halting。

QEMU 需启用 VirtIO MMIO v2（脚本已加 `-global virtio-mmio.force-legacy=false`）。仅跑 bridge 自测：

```bash
python3 tools/deepseek-bridge.py
```

## 文档

| 文件 | 内容 |
|------|------|
| [docs/DESIGN.md](docs/DESIGN.md) | 设计规格与版本历史 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | 未来方向与分阶段实现计划 |
| [docs/OS_BENCHMARK.md](docs/OS_BENCHMARK.md) | 操作系统对标与度量计划 |
| [docs/V5_IMPLEMENTATION.md](docs/V5_IMPLEMENTATION.md) | v5 Console 分阶段设计 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 整体架构 |
| [docs/AGENT_MODEL.md](docs/AGENT_MODEL.md) | Agent 数据模型 |
| [docs/PRODUCT_ROADMAP.md](docs/PRODUCT_ROADMAP.md) | 统一产品路线图 v0.x → v1.0 |
| [docs/BRAND.md](docs/BRAND.md) | 个人品牌与开源运营 |
| [docs/V0.2_RC.md](docs/V0.2_RC.md) | v0.2-rc 双平台 v7 |
| [docs/V0.1_BETA.md](docs/V0.1_BETA.md) | v0.1-beta GA 子 OS |
| [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) | 第三方依赖与许可 |

## 目录

```
include/agentos.h    共享 syscall 定义
kernel/              内核（vm / sched / virtio / persist / session）
user/                Agent 应用 + libagent + agent_svc.c
linker.ld            内核 @ 0x80200000，用户代码 @ 0x80300000
tools/               DeepSeek bridge
scripts/             QEMU / LLM / v1 启动脚本
```

## 版本路线

| 版本 | 内容 |
|------|------|
| v0.1 | S-mode MVP、协作调度、IPC |
| v0.2 | U-mode + ecall + timer 抢占 |
| v0.3 | 每 Agent SV39 页表 |
| v0.5 | Pi-inspired IPC + LLM 流式 |
| v0.6 | 统一 Tool 网关 + ramfs + policy | ✅ |
| v0.7 | VirtIO block + ramfs 持久化（AOS1） | ✅ |
| v0.8 | storage 服务 Agent + IPC FS | ✅ |
| v1.0 | session 持久化 + demo + 文档 | ✅ |
| v1.1 | IPC copy 边界 + 页故障隔离 + RR 调度 + check-v1 | ✅ |
| v1.2 | Memory+LLM 联合 demo + check-memory + VirtIO MMIO 基址修复 | ✅ |
| v1.3 | Orchestrator 五 Agent + agent_svc RPC + check-orchestrator | ✅ |
| v2.0 | Session read/compact + LLM→session + demo_memory2 + check-memory2 | ✅ |
| v2.1 | Storage v2 req_id RPC + 并发 worker + check-storage2 | ✅ |
| v2.2 | Tool audit + GPIO/HTTP stub + check-tools2 | ✅ |
| v2.3 | v2.x wrap: ramfs64 + uaccess + audit persist + check-wrap | ✅ |
| v3.0 | Demand heap + vm_user_check + EFAULT + demo_vm3 + check-vm3 | ✅ |
| v3.1 | Edge sensor pipeline + persist restore + check-edge | ✅ |
| v3.2 | Model router service + agent_svc_llm + check-router | ✅ |
| v3.3 | Agent ELF load + worker.agent + check-load | ✅ |
| v3.4 | Network service + agent_svc_http + check-net | ✅ |
| v3.5 | Persist AOS2 + migrate + check-persist2 | ✅ |
| v4.0 | Platform HAL + check-platform / check-wrap-x86 | ✅ |
| v4.1 | SMP 双 hart + check-smp | ✅ |
| v4.2 | OTA Agent 包 + check-ota | ✅ |
| v4.3 | Production network + check-net-prod | ✅ |
| v4.3.1 | Native LLM net + check-llm-net | ✅ |
| v4.4 | Human I/F VirtIO-GPU + check-ui | ✅ |
| v5.0 | Console REPL + check-console | ✅ |
| v5.1 | Session + Steer + check-console-session | ✅ |
| v5.2 | Orchestrator CLI + check-console-orch | ✅ |
| v5.3 | Pack Launcher + check-console-pack | ✅ |
| v5.4 | Desktop Shell + check-desktop | ✅ |
| v5.5 | Headless Box + check-box + bench-ipc | ✅ |
| v5.6 | Package Catalog + check-catalog | ✅ |
| v6.0 | Multi-tenant + check-console-tenant | ✅ |
| v6.1 | Audit partition + check-console-audit | ✅ |
| v6.2 | Namespace mount + check-console-namespace | ✅ |
| v6.3 | Resource quota + check-console-quota | ✅ |
| v6.4 | x86 Console 子集 + check-console-x86 | ✅ |
| v6.5 | x86 tenant/audit/namespace + check-console-x86-all | ✅ |
| **v0.2-rc** | **双平台 v7 + policy load + check-v0.2-rc** | **✅ (semver 0.2.0)** |
| **0.3.0** | **Fleet ingest + Remote ping + check-0.3.0** | **✅** |
| **v0.1-beta** | **x86 GA 子 OS（v7 全栈）+ check-v0.1-beta** | **✅ (semver 0.1.0)** |
| v7.0 | Fleet 遥测（合入 v0.1-beta） | ✅ |
| v7.1 | Remote Console stub（合入 v0.1-beta） | ✅ |
| v7.2 | Policy-as-Code（合入 v0.1-beta） | ✅ |
| v7.3 | Agent Mesh MVP（合入 v0.1-beta） | ✅ |
| **GA 1.0** | 生产 LTS（目标 2027 Q3） | 规划 |

## License

OpenAgentOS is licensed under the [Apache License 2.0](LICENSE).
See [NOTICE](NOTICE) and [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) for third-party components.
