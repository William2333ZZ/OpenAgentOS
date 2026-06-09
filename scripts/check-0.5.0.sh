#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-v050.elf"
LOG="$(mktemp /tmp/agentos-050-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-050-stdin.XXXXXX).fifo"
GATEWAY_PID=""
COLLECTOR_PID=""

cd "$ROOT"

if [[ ! -f .env ]] && [[ -z "${DEEPSEEK_API_KEY:-}" ]]; then
  echo "[check-0.5.0] missing DEEPSEEK_API_KEY (.env or env var required for direct API)"
  exit 1
fi

chmod +x scripts/mk-deepseek-key.sh
./scripts/mk-deepseek-key.sh
make kernel-console-v050.elf

cleanup() {
  rm -f "$FIFO_IN"
  for pid in "$GATEWAY_PID" "$COLLECTOR_PID"; do
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
}
trap cleanup EXIT

python3 "$ROOT/tools/remote-gateway.py" --port 5557 >"${LOG}.gw" 2>&1 &
GATEWAY_PID=$!
python3 "$ROOT/tools/fleet-collector.py" --host 0.0.0.0 --port 8765 >"${LOG}.fc" 2>&1 &
COLLECTOR_PID=$!
sleep 0.8

mkfifo "$FIFO_IN"

echo "[check-0.5.0] launching QEMU (OpenAgentOS 0.5.0 direct DeepSeek API) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=net0 -device virtio-net-device,netdev=net0 \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 14
  printf '/remote enable\r'
  printf '/remote connect\r'
  printf '/fleet push\r'
  printf '/fleet ingest http://10.0.2.2:8765/ingest\r'
  printf '/llm What is 17+25? Reply with only the number.\r'
  printf '/quit 0.5.0\r'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "0.5.0 demo complete" 480 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS 0.5.0" \
           "\\[virtio-net\\] driver ready" \
           "\\[remote\\] tcp ok" \
           "\\[fleet\\] ingest net ok" \
           "\\[router\\] backend=deepseek" \
           "\\[deepseek\\] answer len=" \
           "answer: 42" \
           "0.5.0 demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-0.5.0] missing: $pat"
    fail=1
  fi
done

if rg -q "host gw|deepseek-net-gw|10\\.0\\.2\\.2:8443" "$LOG" 2>/dev/null; then
  echo "[check-0.5.0] unexpected host gateway fallback in log"
  fail=1
fi

if ! rg -q "fleet-collector.*ingest" "${LOG}.fc" 2>/dev/null; then
  echo "[check-0.5.0] missing: fleet-collector ingest log"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "[check-0.5.0] failed; log=$LOG"
  tail -200 "$LOG"
  tail -30 "${LOG}.fc" 2>/dev/null || true
  tail -20 "${LOG}.gw" 2>/dev/null || true
  exit 1
fi

rm -f "$LOG" "${LOG}.fc" "${LOG}.gw"
echo "[check-0.5.0] ok (direct DeepSeek HTTPS API, no host bridge)"
