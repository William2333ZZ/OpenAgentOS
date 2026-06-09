#!/usr/bin/env bash
# OpenAgentOS 0.6.1 — HTTPS fleet + router faux/deepseek
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-v061.elf"
LOG="$(mktemp /tmp/agentos-061-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-061-stdin.XXXXXX).fifo"
TLS_PID="" COLLECTOR_PID="" GATEWAY_PID=""

cd "$ROOT"
chmod +x scripts/mk-fleet-tls-cert.sh
./scripts/mk-fleet-tls-cert.sh
make kernel-console-v061.elf

cleanup() {
  rm -f "$FIFO_IN"
  for pid in "$TLS_PID" "$COLLECTOR_PID" "$GATEWAY_PID"; do
    [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT

python3 "$ROOT/tools/fleet-collector-tls.py" --port 8443 >"${LOG}.tls" 2>&1 &
TLS_PID=$!
python3 "$ROOT/tools/remote-gateway.py" --port 5557 >"${LOG}.gw" 2>&1 &
GATEWAY_PID=$!
python3 "$ROOT/tools/fleet-collector.py" --host 0.0.0.0 --port 8765 >"${LOG}.fc" 2>&1 &
COLLECTOR_PID=$!
sleep 1

mkfifo "$FIFO_IN"
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 16
  printf '/remote enable\r'
  printf '/remote connect\r'
  printf '/fleet push\r'
  printf '/fleet ingest https://10.0.2.2:8443/ingest\r'
  printf '/llm --backend faux What is 17+25? Reply with only the number.\r'
  printf '/quit 0.6.1\r'
  while kill -0 "$QPID" 2>/dev/null; do sleep 0.2; done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "0.6.1 demo complete" 480 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS 0.6.1" \
           "\\[fleet\\] ingest tls ok" \
           "\\[router\\] backend=faux" \
           "answer: 42" \
           "0.6.1 demo complete"; do
  rg -q "$pat" "$LOG" 2>/dev/null || { echo "[check-0.6.1] missing: $pat"; fail=1; }
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-0.6.1] failed; log=$LOG"
  tail -200 "$LOG"
  exit 1
fi
rm -f "$LOG" "${LOG}.tls" "${LOG}.gw" "${LOG}.fc"
echo "[check-0.6.1] ok (HTTPS fleet + router faux/deepseek)"
