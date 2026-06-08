# OpenAgentOS 产品路线图（统一版）

日期：2026-06-08  
维护者：OpenAgentOS 项目  
相关：[ROADMAP.md](./ROADMAP.md) · [V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md) · [V0.1_BETA.md](./V0.1_BETA.md) · [BRAND.md](./BRAND.md)

---

## 1. 两套版本号：工程 vs 产品

| 维度 | 命名 | 示例 | 用途 |
|------|------|------|------|
| **工程迭代** | v6.x / v7.x / v8.x | v7.0 Fleet | 内核模块、Tool ID、设计文档 |
| **产品发布** | SemVer `MAJOR.MINOR.PATCH` | `0.1.0` → `0.2.0` → `0.3.0` → **1.0.0** | GitHub Release、对外宣传、ABI 冻结 |

```mermaid
flowchart LR
  subgraph eng [工程线]
    v65[v6.5 x86 Console]
    v70[v7.0 Fleet]
    v71[v7.1 Remote]
    v72[v7.2 Policy]
    v73[v7.3 Mesh]
    v80[v8 安全/BSP]
  end
  subgraph prod [产品线]
    b01[0.1.0 ✅]
    rc02[0.2.0 ✅]
    rc03[0.3.0 ✅]
    rc04[0.4.0 ✅]
    rc05[0.5.0 ✅]
    ga10[1.0.0 GA]
  end
  v65 --> b01
  v70 --> b01
  v71 --> b01
  v72 --> rc02
  v73 --> rc02
  v80 --> ga10
  rc02 --> rc03 --> rc04 --> rc05 --> ga10
```

**原则**：工程可以并行；产品 Release 只打包「可验收、可演示、可写文档」的组合。

---

## 2. 已交付：0.1.0（2026-06，原 v0.1-beta）

| 项 | 状态 |
|----|------|
| 平台 | x86_64-pc QEMU 子 OS |
| 能力 | v6 全家桶 + v7 Fleet/Policy/Remote/Mesh |
| 验收 | `make check-0.1.0`（别名 `check-v0.1-beta`） |
| 许可 | Apache-2.0 · [GitHub](https://github.com/William2333ZZ/OpenAgentOS) |

---

## 3. 已交付：0.2.0（2026-07，原 v0.2-rc）

**主题**：双平台 v7 + 策略可加载 + 分项验收

| 子项 | 交付 | 验收 |
|------|------|------|
| v7 RISC-V | `kernel-console-v7.elf` | `make check-console-v7` |
| x86 Fleet 子集 | 独立脚本 | `make check-fleet-x86` |
| Policy 文件加载 | 种子 `/sys/policy/active` + `/policy load` | `make check-0.2.0` |
| Fleet Collector | `tools/fleet-collector.py` | 文档 + 手动 POST |
| 产品内核 | `kernel-x86-v02-rc.elf` | `make check-0.2.0` |

---

## 4. 已交付：0.3.0（2026-06）

**主题**：Fleet HTTP ingest + Remote ping（HTTP_FAUX，VirtIO-net 前奏）

| 子项 | 交付 | 验收 |
|------|------|------|
| Fleet ingest | `/fleet ingest [url]`、`FLEET_CMD_INGEST` | `make check-0.3.0` |
| Remote ping | `/remote ping`、`REMOTE_CMD_PING` | `make check-0.3.0` |
| semver Banner | `0.1.0` / `0.2.0` / `0.3.0` | 见 [VERSIONING.md](./VERSIONING.md) |
| 产品内核 | `kernel-x86-0.3.0.elf` | `make check-0.3.0` |

**非目标（0.3.0 不做）**：VirtIO-net 生产栈、Remote TCP 真链路（留待 0.4+）。

---

## 5. 已交付：0.4.0（2026-06）

**主题**：VirtIO-net Fleet HTTP POST + Remote TCP 真链路（RISC-V 验收）

| 子项 | 交付 | 验收 |
|------|------|------|
| VirtIO-net MMIO | RISC-V `virtio-net-device` + slirp | `make check-0.4.0` |
| Fleet ingest（真 HTTP） | `net_http_post` → collector | `make check-0.4.0` |
| Remote TCP | `/remote connect`、`REMOTE_CMD_CONNECT` | `make check-0.4.0` |
| 产品内核 | `kernel-console-v040.elf` | `make check-0.4.0`（别名 `check-remote-console`） |
| x86 实验 | `kernel-x86-0.4.0.elf` + PCI net 驱动 | 手动 `make run-x86-0.4.0`（RX 待完善） |

**非目标（0.4.0 不做）**：TLS/HTTPS 生产栈、x86 PCI net 作为 Release gate。

---

## 6. 已交付：0.5.0（2026-06）

**主题**：Console `/llm` DeepSeek（VirtIO-net + router + mbedTLS）

| 子项 | 交付 | 验收 |
|------|------|------|
| model-router | `router_svc` + `CAP_LLM` | `make check-0.5.0` |
| DeepSeek | `llm_deepseek` + host gw 回退 | `make check-0.5.0` |
| Console `/llm` | backend=deepseek，无 key 时 faux | `make check-0.5.0` |
| 产品内核 | `kernel-console-v050.elf` | `make check-0.5.0`（别名 `check-llm-console`） |

**继承 0.4.0**：fleet ingest、remote TCP 仍包含在同一验收脚本中。

---

## 7. 0.6+ → 1.0.0 GA 路径

| 版本 | 时间（基准） | 主题 | 退出标准 |
|------|-------------|------|----------|
| **0.6.0** | 2026 Q4 | x86 VirtIO-net PCI 与 RISC-V parity | `check-x86-0.4.0` 绿 |
| **0.9.0** | 2027 Q2 | OTA 签名强制 + 渗透测试修复 | 安全审计清单 |
| **1.0.0 GA** | 2027 Q3 | stable/LTS 渠道、集成商文档、≥1 真板 smoke | 见 [V7_IMPLEMENTATION.md](./V7_IMPLEMENTATION.md) §6.2 |

### v8 工程线（支撑 v1.0）

- 启动链签名、看门狗、audit 防篡改
- ≥2 BSP（RISC-V SBC + x86 工控）
- CI 三层：fast (~2min) / standard (~10min) / nightly

---

## 8. 能力矩阵（规划）

| 能力 | 0.1.0 | 0.2.0 | 0.3.0 | 0.4.0 | 0.5.0 | 1.0.0 |
|------|-----------|---------|---------|---------|---------|------|
| x86 Console 子 OS | ✅ | ✅ | ✅ | ✅ exp | ✅ exp | ✅ |
| RISC-V v7 栈 | stub | ✅ | ✅ | ✅ | ✅ | ✅ |
| Policy 文件 load | 内存 deny | ✅ ramfs | ✅ | ✅ | ✅ | ✅ |
| Fleet HTTP push | stub | collector | ingest (faux) | ingest (net) | ingest (net) | 生产 TLS |
| Remote Console | host stub | stub | ping (faux) | TCP connect | TCP connect | token+审计 |
| Console /llm DeepSeek | faux | faux | faux | faux | ✅ net | ✅ |
| Mesh 跨设备 | beacon | beacon | host relay | host relay | host relay | 可选 |
| 真板 | — | — | smoke | smoke | 必选 |

---

## 8. 验收命令速查

```bash
# 产品 Release gate（SemVer）
make check-0.1.0          # 别名 check-v0.1-beta
make check-0.2.0          # 别名 check-v0.2-rc
make check-0.3.0          # fleet ingest + remote ping
make check-0.4.0          # 别名 check-remote-console：RISC-V virtio-net + TCP (~13s)
make check-0.5.0          # 别名 check-llm-console：DeepSeek /llm + 0.4.0 栈 (~20s)

# v7 分项
make check-console-v7     # RISC-V v7
make check-fleet-x86      # x86 Fleet 子集

# 回归
make check-console-x86-all check-platform
```

---

## 9. ABI 与破坏性变更

- **v0.x**：Tool 22–25 已分配，仅追加不修改语义
- **v1.0**：syscall + Tool 表冻结声明；breaking 仅 major
- 每个产品 Release 更新 [SYSCALL.md](./SYSCALL.md) 与 Release Notes

---

## 10. 与个人品牌的关系

产品路线图由 **OpenAgentOS** 项目承载；个人品牌负责叙事、信任与社区。详见 **[BRAND.md](./BRAND.md)**。

| 层 | 载体 |
|----|------|
| 项目 | `William2333ZZ/OpenAgentOS`、Releases、CI 绿 |
| 个人 | GitHub Profile、技术文章、演示视频、会议分享 |
| 叙事 | 「Agent 原生 OS 从 0 到可运行子系统」 |

建议每个 **产品 minor**（v0.2、v0.3）配一篇 **Story 文章 + 60s 录屏 + Release Note**，形成可检索的时间线。
