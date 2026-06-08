#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-quota.elf"
LOG="$(mktemp /tmp/agentos-console-quota-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-quota-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-quota.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-quota] launching QEMU (resource quota) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/quota status\n'
  printf '/quota burn 16\n'
  printf '/quota status\n'
  printf '/quota probe\n'
  printf '/quit quota\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo quota complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" \
           "\\[quota\\] ipc=16 tool=128 fs_files=32 per agent" \
           "\\[quota\\] agent=10 ipc=16/16 tool=0/128 fs=0/32" \
           "ipc exceeded" \
           "\\[quota\\] limit ok" \
           "\\[quota\\] probe agent=10 ipc rc=-4" \
           "console demo quota complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-quota] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-quota] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-quota] ok (ipc quota burn + probe ENOSPC)"
