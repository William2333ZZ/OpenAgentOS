#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-v064.elf"
LOG="$(mktemp /tmp/agentos-064-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-064-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-v064.elf
mkfifo "$FIFO_IN"

qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 12
  printf '/sandbox status\r'
  printf '/quit 0.6.4\r'
  while kill -0 "$QPID" 2>/dev/null; do sleep 0.2; done
) >"$FIFO_IN" &
WRITER=$!

wait_console_qemu "$LOG" "$QPID" "0.6.4 demo complete" 240
stop_console_qemu "$QPID" "$WRITER"

for pat in "OpenAgentOS 0.6.4" "\\[sandbox\\]" "\\[policy\\]" "0.6.4 demo complete"; do
  rg -q "$pat" "$LOG" || { echo "[check-0.6.4] missing $pat"; tail -80 "$LOG"; exit 1; }
done
rm -f "$LOG"
echo "[check-0.6.4] ok (/sandbox status)"
