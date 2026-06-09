# OpenAgentOS 版本规范

OpenAgentOS 采用 [语义化版本](https://semver.org/lang/zh-CN/)（SemVer）：`MAJOR.MINOR.PATCH`。

**迭代总览**：[RELEASE_ITERATIONS.md](./RELEASE_ITERATIONS.md) · **0.6.x 线**：[V0.6.x.md](./V0.6.x.md) · **1.0 GA**：[V1.0.0.md](./V1.0.0.md) · **Sandbox**：[SANDBOX.md](./SANDBOX.md)

---

## 0.x 阶段版本规则

| 段 | 规则 | 示例 |
|----|------|------|
| **MINOR** | 产品主题冻结一整条线 | `0.6` = Agent Sandbox Runtime |
| **PATCH** | 每个可验收增量 +1 | `0.6.0` → `0.6.1` → `0.6.2` |
| **MAJOR** | ABI / 产品 GA | **`1.0.0`** QEMU GA |

同一 minor 内：**只增能力、不改 Tool 语义**；patch 发 Release，不跳号。

---

## 源码中的版本

- 产品版本：`include/version.h` → `OPENAGENTOS_VERSION`（当前 **1.0.0**）
- 平台 Banner：`-DAGENTOS_VERSION=\"1.0.0\"` 等
- Fleet JSON 字段 `"v"` 同上

---

## 验收目标

### 已交付（产品 semver）

| 版本 | 内核 / 产物 | check | 说明 |
|------|-------------|-------|------|
| 0.4.0 | `kernel-console-v040.elf` | `check-0.4.0` | RV VirtIO-net |
| 0.5.0 | `kernel-console-v050.elf` | `check-0.5.0` | RV DeepSeek /llm |
| 0.6.0 | `kernel-x86-0.6.0.elf` | `check-0.6.0` | x86 sandbox-net |
| 0.6.2 | `kernel-console-v062.elf` + x86 | `check-0.6.2` | 双平台 + router faux |
| 0.6.3 | `kernel-console-v063.elf` | `check-0.6.3` | net egress policy |
| 0.6.4 | `kernel-console-v064.elf` | `check-0.6.4` | `/sandbox status` |
| 0.7.0 | `kernel-ota.elf` + sign | `check-0.7.0` | OTA format-2 HMAC |
| **1.0.0** | `kernel-console-v100.elf` | **`check-ga-1.0`** | **GA 聚合 gate** |

（0.1.0 – 0.3.0、v0.1-beta 等见历史表 / README。）

### 进行中 / 已知问题

| 版本 | 状态 | 说明 |
|------|------|------|
| **0.6.1** | ⚠️ | HTTPS fleet ingest TLS 路径 page fault；HTTP + faux router 已实现 |

---

## 0.6.x — Agent Sandbox（已完结 → 1.0）

| Patch | 状态 | 主题 | 文档 |
|-------|------|------|------|
| 0.6.0 | ✅ | x86 sandbox-net | [V0.6.0.md](./V0.6.0.md) |
| 0.6.1 | ⚠️ | HTTPS fleet + router | [V0.6.1_PLAN.md](./V0.6.1_PLAN.md) |
| 0.6.2 | ✅ | 双平台 parity | [V0.6.2_PLAN.md](./V0.6.2_PLAN.md) |
| 0.6.3 | ✅ | egress 白名单 | [V0.6.3_PLAN.md](./V0.6.3_PLAN.md) |
| 0.6.4 | ✅ | `/sandbox status` | [V0.6.4_PLAN.md](./V0.6.4_PLAN.md) |

---

## 1.0.x

| 版本 | 主题 | 文档 |
|------|------|------|
| **1.0.0** | QEMU GA（MAJOR bump） | [V1.0.0.md](./V1.0.0.md) |
| 0.7.0 | Security：OTA HMAC（0.x minor） | [V0.7.0.md](./V0.7.0.md) |
| LTS 扩展 | 真板 + SLA | [V1.0.0_PLAN.md](./V1.0.0_PLAN.md) |

---

## Release 文档

| 类型 | 路径 |
|------|------|
| GA | [V1.0.0.md](./V1.0.0.md) |
| 0.6.x / 0.7 | [V0.6.0.md](./V0.6.0.md) · [V0.7.0.md](./V0.7.0.md) |
| 规划 | [V0.6.x.md](./V0.6.x.md) · [V0.6.1_PLAN.md](./V0.6.1_PLAN.md) … |
| Sandbox | [SANDBOX.md](./SANDBOX.md) |

---

## GitHub Release

建议标签顺序：`0.6.0` … `0.6.4` → `0.7.0` → **`1.0.0`**。

每个 patch：**check 绿 → bump version.h → Release Note → 可选 60s 录屏**。  
GA 发布前跑 **`make check-ga-1.0`**；并行跑多个 check 时注意释放 **8765 / 5557 / 8443** 端口。
