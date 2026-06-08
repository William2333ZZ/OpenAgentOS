# Agent 模型

## Agent 是什么

在 AgentOS 中，**Agent** 是可调度、可通信、带权限的执行单元，类比传统 OS 中的进程，
但语义面向 AI Agent：拥有独立栈、邮箱、能力集和可选的 Tool / LLM 访问权。

## Agent 控制块 (ACB)

```c
struct agent {
    int              id;         // 全局唯一 ID
    char             name[32];
    enum agent_state state;      // 调度状态
    enum agent_phase phase;      // turn / tool / llm / wait-ipc
    struct trapframe *tf;
    pagetable_t      pagetable;
    uint32_t         caps;
    struct msgbox    inbox;      // steer 队列 + normal 队列
    int              exit_code;
};
```

## 生命周期（调度状态）

```
  CREATE ──► RUNNABLE ◄──► RUNNING
                │              │
                │         recv (EAGAIN + yield)
                │              ▼
                │         phase=WAIT_IPC
                │              │
                ▼              ▼
             ZOMBIE ◄──── EXIT
```

`agent_recv` 在邮箱空时返回 `EAGAIN`（不阻塞内核）；用户态 `agent_recv_msg()` 循环 yield。
收到 IPC 消息时 `agent_send` 将目标置为 `RUNNABLE`。

## Phase（pi AgentHarness 对齐）

| Phase | 含义 |
|-------|------|
| `IDLE` | 空闲 |
| `TURN` | 处理消息 / 用户逻辑 |
| `TOOL` | 执行 `agent_tool` |
| `LLM_WAIT` | 阻塞于 `agent_llm` |
| `WAIT_IPC` | `agent_recv` 无消息（EAGAIN） |

内核在 syscall 入口自动更新 phase；用户可通过 `sys_agent_get_phase()` 读取。

## Capability（能力）

| 位 | 常量 | 说明 |
|----|------|------|
| 0 | `CAP_LOG` | UART 输出 |
| 1 | `CAP_MATH` | `TOOL_ADD` |
| 2 | `CAP_SPAWN` | `agent_create` |
| 3 | `CAP_SEND` | `agent_send` |
| 4 | `CAP_RECV` | `agent_recv` / `agent_compact` |
| 5 | `CAP_LLM` | `agent_llm` / `TOOL_LLM` |
| 6 | `CAP_FS` | `TOOL_READ` / `TOOL_WRITE` |
| 7 | `CAP_TIME` | `TOOL_TIME` |
| 8 | `CAP_SVC_STORAGE` | storage 服务：代理 ramfs 读写 |
| 11 | `CAP_SVC_LLM` | model-router 服务：代理 LLM 推理 |
| 12 | `CAP_SVC_NET` | network 服务：代理 HTTP 请求 |

## 消息模型（pi steer / follow-up）

邮箱分为 **steer 队列**（高优先级，容量 4）与 **normal 队列**（容量 16）。

| Type | 值 | 队列 | 语义 |
|------|-----|------|------|
| 用户自定义 | `1 … 0x7fff` | normal | 应用协议 |
| `MSG_STEER` | `0x8001` | steer | 运行中改道（pi steering） |
| `MSG_FOLLOWUP` | `0x8002` | normal | turn 结束后追加 |
| `MSG_SUMMARY` | `0x8003` | normal | compaction 摘要 |
| `MSG_PIPELINE_DONE` | `0x8004` | normal | pipeline / 服务 shutdown |
| `MSG_FS_READ` | `0x8010` | normal | storage IPC：读路径 |
| `MSG_FS_WRITE` | `0x8011` | normal | storage IPC：`path\|data` |
| `MSG_FS_RSP` | `0x8012` | normal | storage 响应 |
| `MSG_LLM_REQ` | `0x8020` | normal | router IPC：`L{req_id}\|{backend}\|{prompt}` |
| `MSG_LLM_RSP` | `0x8021` | normal | router 响应：`L{req_id}\|{text}` 或 `err:*` |

`agent_recv` **始终先取 steer 队列**，再取 normal。

Compaction（`sys_agent_compact(keep)`）将 normal 队列中除最近 `keep` 条外的消息合并为一条 `MSG_SUMMARY`。

## Storage 服务 Agent

- 固定 id = `STORAGE_AGENT_ID`（2），内核 `agent_start_service()` 注册
- 持有 `CAP_SVC_STORAGE | CAP_FS`，经 Tool 网关访问 ramfs
- 普通 Agent 用 `agent_svc_read/write()` 发 `MSG_FS_*`，无需 `CAP_FS`

## Model Router 服务 Agent (v3.2)

- 固定 id = `ROUTER_AGENT_ID`（6），内核 `agent_start_service()` 注册
- 持有 `CAP_SVC_LLM | CAP_LLM`，是唯一可直接调用 `agent_llm` 的用户 Agent
- 普通 Agent 用 `agent_svc_llm(prompt, buf, len, backend)` 发 `MSG_LLM_REQ`
- `backend`：`faux`（本地/QEMU faux）、`deepseek`（VirtIO 宿主机）、`offline`（拒绝，返回 `err:offline`）

## 动态加载 Agent (v3.3)

- `agent_load(path, name, caps)` → `SYS_AGENT_LOAD`：从 ramfs 读 ELF，内核 `elfload.c` 解析 PT_LOAD 并映射到该 Agent 页表
- 验收：`worker.agent` @ `0x80400000` 经 init 写入 `/agent/1/worker.agent` 后加载；`make check-load` 双启动
- 与静态 init/worker（`0x80300000`）并存；OTA 前置能力

## Network 服务 Agent (v3.4)

- 固定 id = `NET_AGENT_ID`（7），内核 `agent_start_service()` 注册
- 持有 `CAP_SVC_NET | CAP_NET`，是唯一可直接调用 `TOOL_HTTP` 的用户 Agent
- 普通 Agent 用 `agent_svc_http(url, buf, len, backend)` 发 `MSG_HTTP_REQ`
- `backend`：`faux`（内核 stub）、`bridge`（VirtIO console + `tools/http-bridge.py`）、`offline`（拒绝）

## Agent 与 LLM

推理在 **host** 执行（VirtIO JSONL 流式或 faux 模式）；v3.2 起客户端经 **model-router** IPC，内核仍在 router Agent 上下文执行 `agent_llm_query`。

## 命名约定

- Agent ID `0`：idle 占位
- ID `2`：storage 服务（v1 demo）
- ID `6`：model-router 服务（v3.2）
- ID `7`：network 服务（v3.4）
- 用户 Agent：从 `1` 递增（init=1，worker 通常 ≥3）
- 名称最长 31 字符 + NUL
