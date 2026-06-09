#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-v063.elf"
LOG="$(mktemp /tmp/agentos-063-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-063-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-v063.elf
mkfifo "$FIFO_IN"

python3 "$ROOT/tools/fleet-collector.py" --host 0.0.0.0 --port 8765 >/dev/null 2>&1 &
COLLECTOR_PID=$!
sleep 0.5
trap 'kill $COLLECTOR_PID 2>/dev/null; rm -f "$FIFO_IN"' EXIT

qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 14
  printf '/policy net restrict\r'
  printf '/policy net allow 10.0.2.2:8765\r'
  printf '/policy net allow api.deepseek.com:443\r'
  printf '/fleet push\r'
  printf '/fleet ingest http://10.0.2.2:8765/ingest\r'
  printf '/quit 0.6.3\r'
  while kill -0 "$QPID" 2>/dev/null; do sleep 0.2; done
) >"$FIFO_IN" &
WRITER=$!

wait_console_qemu "$LOG" "$QPID" "0.6.3 demo complete" 360
stop_console_qemu "$QPID" "$WRITER"

for pat in "OpenAgentOS 0.6.3" "\\[policy\\] net restrict=1" \
           "\\[fleet\\] ingest net ok" "0.6.3 demo complete"; do
  rg -q "$pat" "$LOG" || { echo "[check-0.6.3] missing $pat"; tail -120 "$LOG"; exit 1; }
done
rm -f "$LOG"
echo "[check-0.6.3] ok (network egress policy)"
