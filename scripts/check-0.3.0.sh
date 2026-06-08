#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-0.3.0.elf"
LOG="$(mktemp /tmp/agentos-030-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-030-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-0.3.0.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-0.3.0] launching QEMU (OpenAgentOS 0.3.0) ..."
qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 3
  printf '/fleet push\n'
  printf '/remote enable\n'
  printf '/remote ping\n'
  printf '/fleet ingest\n'
  printf '/remote status\n'
  printf '/quit 0.3.0\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "0.3.0 demo complete" 180 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS 0.3.0" \
           "\\[fleet\\] push ok" \
           "\\[http\\] faux url=.*ingest" \
           "\\[fleet\\] ingest ok" \
           "\\[remote\\] ping ok" \
           "0.3.0 demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-0.3.0] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-0.3.0] failed; log=$LOG"
  tail -160 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-0.3.0] ok (fleet ingest + remote ping)"
