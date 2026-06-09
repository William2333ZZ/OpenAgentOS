#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[check-ga-1.0] OpenAgentOS GA gate ..."

make check-0.6.0
make check-0.6.4
make check-0.7.0

# Quick RV smoke (skip long HTTPS/LLM in CI aggregate)
make kernel-console-v100.elf
LOG="$(mktemp /tmp/agentos-100-smoke.XXXXXX)"
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  -kernel kernel-console-v100.elf >"$LOG" 2>&1 &
QPID=$!
sleep 8
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true
rg -q "OpenAgentOS 1.0.0" "$LOG" || { echo "[check-ga-1.0] missing 1.0.0 banner"; tail -40 "$LOG"; exit 1; }
rm -f "$LOG"

echo "[check-ga-1.0] ok (OpenAgentOS 1.0.0 GA gate)"
