# AgentOS 架构设计

## 愿景

AgentOS 是以 **Agent** 为一等公民的操作系统：传统 OS 中的进程、IPC、调度、权限，
在 AgentOS 中分别对应 Agent 实体、消息传递、Agent 调度器、Capability（能力令牌）。

底层运行在 **RISC-V 或 x86** 硬件之上（v4.0 起双参考架构）；Agent 不是替代内核，而是替代 **进程抽象**。

## 分层架构

```
┌──────────────────────────────────────────────────────────┐
│  Agent 应用层 / .agent OTA 包                             │
├──────────────────────────────────────────────────────────┤
│  libagent — Agent 运行时库（syscall ABI 跨平台一致）       │
├──────────────────────────────────────────────────────────┤
│  Agent 内核（ARCH 无关）                                   │
│  调度 · IPC · Capability · Tool 网关 · VM · persist       │
├──────────────────────────────────────────────────────────┤
│  HAL — 陷阱 · UART · 定时器 · 中断 · VirtIO 探测          │
├──────────────────────────────────────────────────────────┤
│  BSP / 参考平台                                           │
│  QEMU riscv64 virt │ QEMU x86_64 pc │ （可选真实板）      │
└──────────────────────────────────────────────────────────┘
```

## 核心设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 目标平台 | **RISC-V `virt`（默认）+ x86 PC（v4.0）** | 边缘板卡 + 开发者桌面双覆盖 |
| 启动方式 | RV：OpenSBI → 内核；x86：Multiboot2 stub | 各架构标准路径 |
| 特权级 | 内核 + U-mode Agent | RV：S/U；x86：ring0/3 等价模型 |
| 调度 | 协作式 yield + timer 抢占 | yield/exit 后启用定时器中断 |
| 地址空间 | 每 Agent 独立页表 | RV：SV39；x86：32/64 bit 分页（按平台） |
| IPC | 每 Agent 环形消息队列 | 轻量、可预测、易调试 |
| 权限 | Capability bitmask | 细粒度 Tool 授权 |
| 平台移植 | HAL + `PLATFORM=` 构建 | 不换 Agent ABI / AOS2 / OTA 格式 |

## 数据流：Agent 协作示例

```
  [Planner Agent]
        │ agent_send("analyze:42")
        ▼
  [Worker Agent] ──tool_invoke(LOG)──► [Tool Gateway] ──► UART
        │ agent_send("result:ok")
        ▼
  [Planner Agent] ──agent_recv()──► 继续编排
```

## 内存布局（RISC-V QEMU virt 参考）

| 区域 | 地址 | 说明 |
|------|------|------|
| UART | 0x10000000 | NS16550 控制台 |
| VirtIO MMIO | 0x10001000–0x10008000 | VirtIO 设备（8 槽，QEMU 10.x） |
| RAM 起始 | 0x80000000 | QEMU virt 默认 |
| 内核加载 | 0x80200000 | OpenSBI 跳转地址 |
| 用户代码 | 0x80300000 | pipeline / llm demo |
| 用户栈 VA | 0x40000000+ | 每 Agent `USER_STACK_SLOT`（8KiB 栈 + 4KiB guard） |
| 堆 | 0x80600000 起 | bump allocator |
| 页表池 | 0x80800000 | SV39 页表页 |

x86 PC 参考机布局见 v4.0 平台描述（[ROADMAP.md](./ROADMAP.md) §11）。

## 扩展路线

**当前基线**：**v6.5**（Multi-tenant + Audit/Namespace/Quota + RISC-V/x86 Console 矩阵 ✅）。

**规划路径**：

```
v3.5 persist v2 ✅
  → v4.0 Platform ✅ → v4.1 SMP ✅ → v4.2 OTA ✅ → v4.3 Net ✅
  → v4.3.1 Native LLM Net ✅ → v4.4 Human I/F ✅
  → v5.0–v5.6 Console / Desktop / Catalog ✅
  → v6.0–v6.5 Multi-tenant + x86 Console ✅
  → v7.0 Fleet → v7.1 Remote Console → v7.2 Policy → v8 GA
```

详见 [ROADMAP.md](./ROADMAP.md) · [V6_IMPLEMENTATION.md](./V6_IMPLEMENTATION.md) · [V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md) · [OS_BENCHMARK.md](./OS_BENCHMARK.md)。

## 目录结构

```
risk5agents/
├── docs/           设计文档
├── include/        内核/用户共享头文件
├── kernel/         内核源码
├── user/           Agent 应用与 libagent
├── linker.ld       链接脚本
├── Makefile
└── scripts/        QEMU 启动脚本
```
