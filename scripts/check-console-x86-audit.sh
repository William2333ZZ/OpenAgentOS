#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-console-audit.elf"
LOG="$(mktemp /tmp/agentos-console-x86-audit-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-x86-audit-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-console-audit.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-x86-audit] launching QEMU (x86 audit partition) ..."
qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 3
  i=1
  while [[ "$i" -le 28 ]]; do
    printf '/session append audit-fill-%02d\n' "$i"
    i=$((i + 1))
  done
  printf '/audit status\n'
  i=1
  while [[ "$i" -le 12 ]]; do
    printf '/session append audit-more-%02d\n' "$i"
    i=$((i + 1))
  done
  printf '/audit compact 4\n'
  printf '/audit tail\n'
  printf '/audit probe 1\n'
  printf '/quit audit\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo audit complete" 240 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" \
           "\\[audit\\] tool.log quota=512" \
           "\\[audit\\] agent=10 used=" \
           "quota exceeded" \
           "\\[audit\\] compact agent=10" \
           "\\[audit\\] tail agent=10" \
           "\\[audit\\] isolation ok" \
           "console demo audit complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-x86-audit] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-x86-audit] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-x86-audit] ok (x86 audit quota + compact + isolation)"
