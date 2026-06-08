#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-v7.elf"
LOG="$(mktemp /tmp/agentos-console-v7-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-v7-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-v7.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-v7] launching QEMU (RISC-V v7 stack) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/fleet status\n'
  printf '/fleet push\n'
  printf '/policy deny 10\n'
  printf '/policy probe 10\n'
  printf '/remote enable\n'
  printf '/mesh beacon\n'
  printf '/quit v7\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo v7 complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS" "\\[fleet\\] push ok" \
           "\\[policy\\] deny ok" \
           "\\[remote\\] enable agent=10" \
           "\\[mesh\\] beacon device=" \
           "console demo v7 complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-v7] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-v7] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-v7] ok (RISC-V v7 fleet/policy/remote/mesh)"
