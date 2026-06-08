#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console.elf"
LOG="$(mktemp /tmp/agentos-console-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console] launching QEMU without GPU (UART REPL) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 1.5
  printf '/help\n'
  sleep 1
  printf '/llm What is 17+25? Reply with only the number.\n'
  sleep 2
  printf '/quit\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" "answer: 42" "console demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console] failed; log=$LOG"
  tail -80 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console] ok (headless UART REPL, no virtio-gpu)"
