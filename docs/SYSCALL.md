# AgentOS Syscall ABI

## 调用约定

Agent 运行在 **U-mode**，通过 `ecall` 陷入 S-mode 内核；`stvec` 指向 `trap_vector`。

| 寄存器 | 用途 |
|--------|------|
| `a7` | Syscall 编号 |
| `a0`–`a2` | 参数 1–3（当前实现） |
| `a0` | 返回值（≥0 成功；<0 为负 errno） |

内核在处理 syscall 时设置 `sstatus.SUM`，以便访问用户指针。

## Syscall 一览

| 编号 | 名称 | 参数 | 返回 | 需要 CAP |
|------|------|------|------|----------|
| 1 | `agent_create` | `a0=entry fn`, `a1=name`, `a2=caps` | agent id | `CAP_SPAWN` |
| 2 | `agent_destroy` | `a0=target id` | 0 / errno | — |
| 3 | `agent_send` | `a0=dst id`, `a1=type`, `a2=payload ptr` | 0 / errno | `CAP_SEND` |
| 4 | `agent_recv` | `a0=msg buf ptr` | type / errno | `CAP_RECV` |
| 5 | `agent_yield` | — | 0 | — |
| 6 | `agent_self` | — | self id | — |
| 7 | `agent_write` | `a0=buf`, `a1=len` | bytes written | `CAP_LOG` |
| 8 | `agent_tool` | `a0=tool`, `a1=arg0`, `a2=arg1`, `a3=arg2` | result / errno | tool-specific |
| 9 | `agent_exit` | `a0=exit code` | 不返回（调度下一个） | — |
| 10 | `agent_llm` | `a0=prompt`, `a1=buf`, `a2=buflen` | 字节数 / errno | `CAP_LLM` |
| 11 | `agent_compact` | `a0=keep` | 0 / errno | `CAP_RECV` |
| 12 | `agent_get_phase` | — | phase / errno | — |
| 13 | `agent_sync` | — | 0 / errno | —（将 ramfs 用户文件写入块设备） |
| 14 | `agent_heap_base` | — | 堆基址 / errno | — |
| 15 | `agent_heap_map` | page_off | va / errno | 显式 map 堆页 |
| 16 | `agent_heap_cow` | dst_off, src_off | va / errno | 堆页 COW 共享 |
| 17 | `agent_load` | `a0=path`, `a1=name`, `a2=caps` | agent id / errno | `CAP_SPAWN` |

## 错误码

| 值 | 常量 | 含义 |
|----|------|------|
| -1 | `EPERM` | 无 Capability |
| -2 | `EINVAL` | 参数无效 |
| -3 | `ENOENT` | Agent 不存在 |
| -4 | `ENOSPC` | 邮箱满 / Agent 池满 |
| -5 | `EAGAIN` | 非阻塞 recv 无消息 |
| -6 | `ENODEV` | VirtIO / LLM 通道不可用 |
| -7 | `EIO` | I/O 错误 |
| -8 | `ETIMEDOUT` | LLM 等待超时 |
| -9 | `EFAULT` | 非法用户指针 / 用户内存访问失败 |

## 详细说明

### agent_create

创建新 Agent 并加入 RUNNABLE 队列；分配独立页表与用户栈。

```c
int agent_create(void (*entry)(void), const char *name, uint32_t caps);
```

- `entry`：Agent 入口函数
- 最多同时存在 **12** 个 Agent（含 init）
- 调用者必须持有 `CAP_SPAWN`

### agent_send

向目标 Agent 投递消息。若目标处于 WAITING，唤醒为 RUNNABLE。

```c
int agent_send(int dst, int type, const char *payload);
```

- `payload` 为 **用户态** 字符串指针，最长 223 字符
- 目标邮箱满时返回 `ENOSPC`

### agent_recv

读取一条消息；**steer 队列优先于 normal 队列**。邮箱为空时返回 `EAGAIN` 并将 phase 置为 `WAIT_IPC`。用户态应通过 `agent_recv_msg()` 循环配合 `agent_yield()` 等待。

```c
int agent_recv(struct agent_msg *msg);
```

### agent_yield / 抢占

- 协作：`agent_yield()` 主动让出 CPU
- 抢占：timer 中断在 U-mode 下触发 `schedule_trap()`；yield/exit 后启用 STIE

### agent_compact (v0.5)

将 normal 邮箱中除最近 `keep` 条外的消息合并为一条 `MSG_SUMMARY`（pi compaction 隐喻）。

```c
int agent_compact_inbox(int keep);
```

### agent_get_phase (v0.5)

返回当前 Agent phase：`IDLE | TURN | TOOL | LLM_WAIT | WAIT_IPC`。

### agent_llm (v0.4+)

经 VirtIO JSONL 流式协议将 prompt 发往宿主机 `tools/deepseek-bridge.py`；delta 经 `agent_write` 实时输出。编译时加 `-DLLM_FAUX` 可离线自测。

```c
int agent_llm(const char *prompt, char *buf, int buflen);
```

需要 `CAP_LLM`；首次调用时初始化 VirtIO 通道。

### agent_tool

内置 Tool 网关，内核校验 Capability 后执行。

| Tool ID | 名称 | 需要 CAP | 说明 |
|---------|------|----------|------|
| 0 | `TOOL_LOG` | `CAP_LOG` | 打印整数 arg0 |
| 1 | `TOOL_ADD` | `CAP_MATH` | 返回 arg0 + arg1（policy 限幅） |
| 2 | `TOOL_TIME` | `CAP_TIME` | 读 `time` CSR；arg0 可选 ticks 输出指针 |
| 3 | `TOOL_READ` | `CAP_FS` | arg0=path, arg1=buf, arg2=len |
| 4 | `TOOL_WRITE` | `CAP_FS` | arg0=path, arg1=data, arg2=len |
| 5 | `TOOL_LLM` | `CAP_LLM` | 同 `agent_llm`（统一网关） |
| 6 | `TOOL_SESSION_APPEND` | `CAP_LOG` | arg0=文本；追加到 `/session/agent/<id>/log` |
| 7 | `TOOL_SESSION_TAIL` | `CAP_LOG` | arg0=buf, arg1=buflen；读最后一行 |

路径策略（v0.6+，`kernel/tool.c`）：

- `/sys/*`：所有 Agent 只读
- `/agent/<id>/*`：仅 id 本人可读写
- `/session/agent/<id>/*`：session Tool 内核直写 ramfs（append/tail）

底层存储为内存 ramfs（`kernel/ramfs.c`）；v1.0 起经 VirtIO block 持久化（`kernel/persist.c`），`agent_sync()` 触发落盘。

### agent_sync (v1.0 / v3.5 AOS2)

将 ramfs 中用户文件（非 `/sys/*`）序列化写入 VirtIO 块设备。启动时内核自动 `persist_load()`。

- **v3.5 起**：落盘格式为 **AOS2**（`version=2`）：每文件 meta 含 `crc32`；superblock `checksum` 为各文件校验 xor
- 启动时若磁盘为 **AOS1**（`version=1`），内核自动 `persist_migrate()` 并改写 AOS2
- 日志：`[persist] loading N files from AOS1 block` / `from AOS2 block`

```c
int agent_sync(void);  /* libagent: agent_sync() */
```

需要块设备就绪；无设备时返回 `ENODEV`。

### Storage 服务 Agent (v0.8)

内核启动时注册 **storage agent**（固定 id=`STORAGE_AGENT_ID`=2），持有 `CAP_SVC_STORAGE | CAP_FS`，负责经 Tool 网关访问 ramfs。

Worker 无 `CAP_FS` 时，通过 IPC 委托 storage：

| 消息 | 方向 | payload | 响应 |
|------|------|---------|------|
| `MSG_FS_READ` (0x8010) | client → storage | 路径字符串 | `MSG_FS_RSP` + 文件内容 |
| `MSG_FS_WRITE` (0x8011) | client → storage | `path\|data`（无 `\0`） | `MSG_FS_RSP` + `ok` 或 `err:*` |
| `MSG_FS_RSP` (0x8012) | storage → client | 见上 | — |

libagent 封装：`agent_svc_read()` / `agent_svc_write()`（实现于 `user/agent_svc.c`）。

### Agent 堆与内存安全 (v3.0)

| Syscall | 说明 |
|---------|------|
| `SYS_AGENT_HEAP_BASE` (14) | 返回当前 Agent 堆基址（每 Agent 16 页 @ `0x60000000` 起） |
| `SYS_AGENT_HEAP_MAP` (15) | 显式 map 指定页；缺页时 fault handler 也会按需 map |
| `SYS_AGENT_HEAP_COW` (16) | 将 `dst` 页设为 `src` 页的只读共享映射；写入时 copy-on-write |

`copy_from_user` / `copy_to_user` 对非法用户指针返回 `EFAULT`（`-9`），不再 panic 内核。用户态非法访问触发页故障时，faulting Agent 被 `agent_exit`，其他 Agent 继续运行。

**栈布局**：每 Agent 占用 `USER_STACK_SLOT`（`AGENT_STACK_SIZE + 4KiB`）VA 槽；槽底 4KiB 为 guard 页（不映射），其上是可写栈区。用户代码 W^X：`.user.text`/`.user.rodata` 为 R\|X，`.user.bss` 为 R\|W。

libagent：`agent_heap_base()` / `agent_heap_map_page()` / `agent_heap_cow_page()`（见 `include/agentos.h`）。

### agent_load (v3.3)

从 ramfs 路径加载 **ELF64 RISC-V** 可执行文件，创建独立页表 Agent（入口由 ELF `e_entry` 决定，与静态链接 `@ 0x80300000` 的 demo 可共存）。

```c
int agent_load(const char *path, const char *name, uint32_t caps);
```

- 调用者需 `CAP_SPAWN`；路径经 ramfs 读取（单文件最大 **8KiB**，见 `RAMFS_FILE_SIZE`）
- 仅映射 `PT_LOAD` 段；栈/堆布局与 `agent_create` 相同
- 独立 `.agent` 链接脚本：`linker-agent.ld`（默认 `0x80400000`）
- 非 load 内核链接 `elfload_stub.o`，syscall 返回 `EINVAL`

### Model Router IPC (v3.2)

| 消息 | 说明 |
|------|------|
| `MSG_LLM_REQ` (0x8020) | client → router：`L{req_id}\|{backend}\|{prompt}` |
| `MSG_LLM_RSP` (0x8021) | router → client：`L{req_id}\|{text}` 或 `err:*` |

`backend`：`faux` / `deepseek` / `offline`（offline 返回 `err:offline`）。

libagent：`agent_svc_llm(prompt, buf, len, backend)`（`user/agent_svc.c`）；router 实现于 `user/router_svc.c`（`ROUTER_AGENT_ID=6`）。

### Network HTTP IPC (v3.4)

| 消息 | 说明 |
|------|------|
| `MSG_HTTP_REQ` (0x8022) | client → network：`H{req_id}\|{backend}\|{url}` |
| `MSG_HTTP_RSP` (0x8023) | network → client：`H{req_id}\|{body}` 或 `err:*` |

`backend`：`faux`（本地 stub）、`bridge`（VirtIO console + `http-bridge.py`）、`offline`（拒绝）。

`TOOL_HTTP`（11）需 `CAP_NET`；普通 Agent 经 `agent_svc_http(url, buf, len, backend)` 访问 network 服务（`NET_AGENT_ID=7`）。

v4.3 生产网络：`backend` 扩展 `native` / `prod`（quota + cache fallback），见 `user/network_prod_svc.c`。

### Display / Input IPC (v4.4)

| 消息 | 说明 |
|------|------|
| `MSG_DISPLAY_REQ` (0x8024) | client → display：`D{req_id}\|{x},{y}\|{text}` |
| `MSG_DISPLAY_RSP` (0x8025) | display → client：`D{req_id}\|ok` 或 `err:*` |
| `MSG_INPUT_REQ` (0x8026) | client → input：`I{req_id}\|poll` |
| `MSG_INPUT_RSP` (0x8027) | input → client：`I{req_id}\|none` / `key\|{code}` |

| Tool | CAP | 说明 |
|------|-----|------|
| `TOOL_DISPLAY` (14) | `CAP_DISPLAY` | arg0：0=clear，1=draw_text，2=flush |
| `TOOL_INPUT` (15) | `CAP_INPUT` | arg0=0 poll；返回 Linux keycode（按下）或 0 |

服务 Agent：`DISPLAY_AGENT_ID=8`，`INPUT_AGENT_ID=9`。libagent：`agent_svc_display(text,x,y)`、`agent_svc_input_poll()`。

**GPU 可选（Console 回退）**：`TOOL_DISPLAY` 在 `virtio_gpu` 未就绪时经 UART 输出 `[ui-console]`；`TOOL_INPUT` 无 virtio-keyboard 时读串口（QEMU `-nographic`  stdin）。应用层 RPC 不变。

### Console REPL (v5.0)

| 消息 | 说明 |
|------|------|
| `MSG_CONSOLE_REQ` (0x8028) | client → console（预留） |
| `MSG_CONSOLE_RSP` (0x8029) | console → client（预留） |

| Tool | CAP | 说明 |
|------|-----|------|
| `TOOL_CONSOLE` (16) | `CAP_SVC_CONSOLE` | arg0：0=GETLINE（阻塞，内核内），1=PUTS，2=PROMPT，3=READ_CHAR（非阻塞，0=无数据） |
| `TOOL_CATALOG` (17) | `CAP_FS` | arg0：`CATALOG_CMD_*`；arg1=包名 |
| `TOOL_TENANT` (18) | `CAP_LOG` | arg0：`TENANT_CMD_STATUS`(0) / `PROBE`(1)；arg1=目标 Agent id |
| `TOOL_AUDIT` (19) | `CAP_LOG` | arg0：`AUDIT_CMD_STATUS`(0) / `TAIL`(1) / `PROBE`(2) / `COMPACT`(3)；arg1=tail 字节数 / probe 目标 id / compact 保留行数 |
| `TOOL_NAMESPACE` (20) | `CAP_LOG` | arg0：`NS_CMD_STATUS`(0) / `PROBE`(1) / `WRITE`(2)；arg1=probe 目标 id 或 write 文本指针 |
| `TOOL_QUOTA` (21) | `CAP_LOG` | arg0：`QUOTA_CMD_STATUS`(0) / `BURN`(1) / `PROBE`(2) / `NOTIFY`(3) |

服务 Agent：`CONSOLE_AGENT_ID=10`。REPL 在 `console_svc.c`：

| 版本 | 命令 |
|------|------|
| v5.0 | `/help` `/echo` `/llm` `/agents` `/quit` |
| v5.1 | `/session tail\|append` `/compact [keep]` `/steer` `/quit session` |
| v5.2 | `/run orch` `/status` `/quit orch` |
| v5.3 | `/list` `/load <path> [name]` `/ota verify\|apply` `/quit pack` |
| v6.0 | `/tenant status\|probe <id>` `/quit tenant` |
| v6.1 | `/audit status\|tail\|compact [n]\|probe <id>` `/quit audit` |
| v6.2 | `/namespace status\|write <text>\|probe <id>` `/quit namespace` |
| v6.3 | `/quota status\|burn <n>\|probe` `/quit quota` |

**v5.2/v5.3 与 init 协作**：console 发 `MSG_CONSOLE_REQ` 到 init(id=1)，payload 如 `run:orch`、`load:/path\|name`、`list`。

**v5.3 load 路径**：init `agent_load` 后 worker 发 `MSG_RESULT`；内核 `sched_notify_runnable` 保证 worker 不被 console 空转饿死。

### Desktop Shell (v5.4)

| 消息 | 说明 |
|------|------|
| `MSG_SHELL_REQ` (0x802A) | shell → init，如 `load:worker` |
| `MSG_SHELL_RSP` (0x802B) | init → shell，如 `load ok` / `load failed` |

| CAP | 说明 |
|-----|------|
| `CAP_SVC_SHELL` | shell-agent 服务标记 |

服务 Agent：`SHELL_AGENT_ID=11`。`kernel-desktop.elf`：display(8) + input(9) + shell(11)；init 读 `/sys/gpu`，无 GPU 时 early exit。

**v5.4 load 路径**：shell 按 `1` → `MSG_SHELL_REQ` → init `agent_load` → `MSG_SHELL_RSP` → shell 发 `MSG_RESULT desktop demo complete`。

验收：`make check-desktop`（GPU + monitor `sendkey 1`）；`make check-desktop-console`（无 GPU fallback）。

### Headless Box (v5.5)

| 路径 | 说明 |
|------|------|
| `/sys/headless` | ramfs `1` 表示 headless 形态 |
| `/quit box` | console → init `box demo complete` |

仅启动 console-agent；无 `DISPLAY_AGENT_ID` / `INPUT_AGENT_ID` 服务。验收：`make check-box`；M1 度量：`make bench-ipc`。

### Native LLM Network (v4.3.1)

`kernel-llm-net.elf` 在 Guest 内通过 `netstack` + `tls_client` + `llm_deepseek` 调用 DeepSeek；不新增 syscall。构建时注入 API key（`generated/deepseek_key.h`）；运行时 DNS/DoH 解析，TLS 失败时 HTTP 回退宿主机 gateway。

### v7 Tool（v0.1-beta 已实现，x86 GA）

详见 [V0.1_BETA.md](./V0.1_BETA.md) · [V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md)。

| Tool | ID | 说明 |
|------|-----|------|
| `TOOL_FLEET` | 22 | 遥测：`FLEET_CMD_STATUS` / `PUSH` / `PROBE` |
| `TOOL_POLICY` | 23 | 策略：`POLICY_CMD_STATUS` / `LOAD` / `PROBE` / `DENY` / `ALLOW` |
| `TOOL_REMOTE` | 24 | 远程 Console：`REMOTE_CMD_STATUS` / `ENABLE` / `DISABLE` |
| `TOOL_MESH` | 25 | Mesh MVP：`MESH_CMD_STATUS` / `BEACON` / `PROBE` |

验收：`make check-v0.1-beta`（x86 `kernel-x86-v01-beta.elf`）。

## libagent 封装

用户代码应通过 `user/libagent.h` 调用。常用 API：

- `agent_spawn` / `agent_load` / `agent_send_msg` / `agent_recv_msg`
- `agent_svc_http(url, buf, len, backend)` — 经 network 服务 HTTP GET
- `agent_svc_display(text, x, y)` / `agent_svc_input_poll()` — v4.4 人机界面
- `agent_read_file` / `agent_write_file`（需 `CAP_FS`）
- `agent_session_append` / `agent_session_tail`（需 `CAP_LOG`）
- `agent_sync()` — 持久化 ramfs 用户文件

```c
#include "libagent.h"

void worker(void) {
    agent_write_file("/agent/2/note.txt", "hello", 5);
    agent_session_append("turn1:hello");
    agent_sync();

    struct agent_msg msg;
    agent_recv_msg(&msg);
    agent_log_str(msg.payload);
}
```
