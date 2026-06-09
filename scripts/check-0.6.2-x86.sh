#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-0.6.2.elf"
LOG="$(mktemp /tmp/agentos-062x-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-062x-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-0.6.2.elf
mkfifo "$FIFO_IN"

python3 "$ROOT/tools/remote-gateway.py" --port 5557 >/dev/null 2>&1 &
GATEWAY_PID=$!
python3 "$ROOT/tools/fleet-collector.py" --host 0.0.0.0 --port 8765 >/dev/null 2>&1 &
COLLECTOR_PID=$!
sleep 0.8
trap 'kill $GATEWAY_PID $COLLECTOR_PID 2>/dev/null; rm -f "$FIFO_IN"' EXIT

qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 20
  printf '/remote enable\r/remote connect\r/fleet push\r'
  printf '/fleet ingest http://10.0.2.2:8765/ingest\r'
  printf '/llm --backend faux What is 17+25? Reply with only the number.\r'
  printf '/quit 0.6.2\r'
  while kill -0 "$QPID" 2>/dev/null; do sleep 0.2; done
) >"$FIFO_IN" &
WRITER=$!

wait_console_qemu "$LOG" "$QPID" "0.6.2 demo complete" 360
stop_console_qemu "$QPID" "$WRITER"

for pat in "OpenAgentOS 0.6.2" "\\[router\\] backend=faux" "answer: 42" "0.6.2 demo complete"; do
  rg -q "$pat" "$LOG" || { echo "[check-0.6.2-x86] missing $pat"; tail -100 "$LOG"; exit 1; }
done
rm -f "$LOG"
echo "[check-0.6.2-x86] ok"
