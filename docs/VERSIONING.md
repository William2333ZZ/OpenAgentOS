# OpenAgentOS 版本规范

OpenAgentOS 采用 [语义化版本](https://semver.org/lang/zh-CN/)（SemVer）：`MAJOR.MINOR.PATCH`（例如 `0.3.0`）。

## 产品线 vs 工程线

| 维度 | 命名 | 示例 | 说明 |
|------|------|------|------|
| **产品版本** | `MAJOR.MINOR.PATCH` | `0.1.0`、`0.2.0`、`0.3.0` | 用户可见 Release、Banner、`include/version.h` |
| **工程里程碑** | `v6` / `v7` / `v8` | Console v7、SV39 v8 | 内核/架构演进，见 `docs/PRODUCT_ROADMAP.md` |

旧称 `v0.1-beta`、`v0.2-rc` 仍可作为 Makefile 目标别名，Banner 已统一为 semver。

## 源码中的版本

- 默认产品版本：`include/version.h` → `OPENAGENTOS_VERSION`
- x86 GA 内核 Banner：各 `kernel/platform_x86_v0N.o` 编译时 `-DAGENTOS_VERSION="0.N.0"`
- Fleet JSON 遥测字段 `"v"` 使用 `OPENAGENTOS_VERSION`

## 验收目标

| 版本 | 内核产物 | 验收命令 |
|------|----------|----------|
| 0.1.0 | `kernel-x86-v01-beta.elf` | `make check-0.1.0`（别名 `check-v0.1-beta`） |
| 0.2.0 | `kernel-x86-v02-rc.elf` | `make check-0.2.0`（别名 `check-v0.2-rc`） |
| 0.3.0 | `kernel-x86-0.3.0.elf` | `make check-0.3.0` |

## 0.3.0 能力摘要

- **Fleet ingest**：`/fleet ingest [url]` — 读取 `/agent/0/fleet.json`，经 HTTP_FAUX POST 到 collector（URL 含 `ingest` 即成功）
- **Remote ping**：`/remote ping` — 需先 `/remote enable`，用于远程控制台存活探测

## GitHub Release

Release 标签使用 semver 无前缀：`0.1.0`、`0.2.0`、`0.3.0`。历史 `v0.1-beta` / `v0.2-rc` 标签保留指向同一 commit 时可加 Release Note 说明映射关系。
