#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-pack.elf"
LOG="$(mktemp /tmp/agentos-console-pack-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-pack-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-pack.elf worker.agent user/worker_elf.inc

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-pack] launching QEMU without GPU (pack launcher) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/list\n'
  printf '/load /agent/1/worker.agent worker\n'
  if wait_console_log "$LOG" "$QPID" "\\[pack\\] load ok" 100; then
    printf '/quit pack\n'
  fi
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo pack complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" "worker.agent" \
           "\\[worker\\] sum=42" "\\[pack\\] load ok" "console demo pack complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-pack] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-pack] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-pack] ok (list + load worker.agent + sum=42)"
