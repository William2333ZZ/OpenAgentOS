#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-tenant.elf"
LOG="$(mktemp /tmp/agentos-console-tenant-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-tenant-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-tenant.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-tenant] launching QEMU (multi-tenant) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  i=1
  while [[ "$i" -le 40 ]]; do
    printf '/session append tenant-line-%02d-0123456789\n' "$i"
    i=$((i + 1))
  done
  printf '/tenant status\n'
  printf '/tenant probe 1\n'
  printf '/quit tenant\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo tenant complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" "\\[tenant\\] session quota=640" \
           "quota exceeded" "\\[tenant\\] agent=10 session used=" \
           "\\[tenant\\] isolation ok" "console demo tenant complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-tenant] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-tenant] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-tenant] ok (quota + cross-tenant probe)"
