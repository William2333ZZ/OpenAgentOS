#!/usr/bin/env bash
# OpenAgentOS 0.6.2 — dual-platform sandbox-net
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/scripts/check-0.6.2-riscv.sh"
"$ROOT/scripts/check-0.6.2-x86.sh"
echo "[check-0.6.2] ok (riscv + x86)"
