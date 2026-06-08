#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-v02-rc.elf"
LOG="$(mktemp /tmp/agentos-v02-rc-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-v02-rc-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-v02-rc.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-v0.2-rc] launching QEMU (OpenAgentOS v0.2-rc) ..."
qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 3
  printf '/policy status\n'
  printf '/policy load\n'
  printf '/policy probe 10\n'
  printf '/policy allow 10\n'
  printf '/policy probe 10\n'
  printf '/fleet push\n'
  printf '/remote status\n'
  printf '/mesh status\n'
  printf '/quit rc\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "v0.2-rc demo complete" 180 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS 0.2.0" \
           "\\[policy\\] load agent=10 path=/sys/policy/active rules=1" \
           "\\[policy\\] deny ok" \
           "\\[policy\\] allow ok" \
           "\\[fleet\\] push ok" \
           "\\[remote\\] agent=10 enabled=0" \
           "\\[mesh\\] agent=10 device=" \
           "v0.2-rc demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-v0.2-rc] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-v0.2-rc] failed; log=$LOG"
  tail -160 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-v0.2-rc] ok (policy load + fleet + v0.2-rc stack)"
