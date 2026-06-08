#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-orch.elf"
LOG="$(mktemp /tmp/agentos-console-orch-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-orch-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-orch.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-orch] launching QEMU without GPU (orch REPL) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/status\n'
  printf '/run orch\n'
  if wait_console_log "$LOG" "$QPID" "orch pipeline ok" 100; then
    printf '/quit orch\n'
  fi
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo orch complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" "\\[worker\\] sum=42" \
           "\\[planner\\] orchestrator complete" "console demo orch complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-orch] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-orch] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-orch] ok (console /run orch + worker sum=42)"
