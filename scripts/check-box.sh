#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-box.elf"
LOG="$(mktemp /tmp/agentos-box-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-box-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-box.elf worker.agent user/worker_elf.inc

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-box] launching headless QEMU (console only, no UI agents) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  for _ in $(seq 1 80); do
    if rg -q "agentos>" "$LOG" 2>/dev/null || rg -q "\\[console\\] service ready" "$LOG" 2>/dev/null; then
      break
    fi
    sleep 0.15
  done
  printf '/list\n'
  for _ in $(seq 1 60); do
    if rg -q "\\[pack\\] list:" "$LOG" 2>/dev/null; then
      break
    fi
    sleep 0.15
  done
  printf '/load /agent/1/worker.agent worker\n'
  if wait_console_log "$LOG" "$QPID" "\\[pack\\] load ok" 100; then
    printf '/quit box\n'
  fi
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "box demo complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "headless: no display" "\\[console\\] service ready id=10" \
           "\\[pack\\] list:" "\\[worker\\] sum=42" "\\[pack\\] load ok" "box demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-box] missing: $pat"
    fail=1
  fi
done

if rg -q "service agent id=8|service agent id=9|\\[display\\]|\\[input\\]" "$LOG" 2>/dev/null; then
  echo "[check-box] unexpected UI service agent started"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "[check-box] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-box] ok (headless UART + load worker sum=42)"
