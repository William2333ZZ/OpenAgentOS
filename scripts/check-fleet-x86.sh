#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-v01-beta.elf"
LOG="$(mktemp /tmp/agentos-fleet-x86-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-fleet-x86-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-v01-beta.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-fleet-x86] launching QEMU (x86 fleet subset) ..."
qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 3
  printf '/fleet status\n'
  printf '/fleet push\n'
  printf '/fleet probe http://127.0.0.1:8765/ingest\n'
  printf '/quit beta\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "v0.1-beta demo complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "\\[fleet\\] platform=" \
           "\\[fleet\\] push ok" \
           "\\[fleet\\] probe agent=10 url=" \
           "v0.1-beta demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-fleet-x86] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-fleet-x86] failed; log=$LOG"
  tail -80 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-fleet-x86] ok (x86 fleet status/push/probe)"
