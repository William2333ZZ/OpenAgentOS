#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-x86-v01-beta.elf"
LOG="$(mktemp /tmp/agentos-v01-beta-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-v01-beta-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-x86-v01-beta.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-v0.1-beta] launching QEMU (x86 v0.1-beta GA) ..."
qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
  <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 3
  printf '/fleet status\n'
  printf '/fleet push\n'
  printf '/policy status\n'
  printf '/policy deny 10\n'
  printf '/policy probe 10\n'
  printf '/remote status\n'
  printf '/remote enable\n'
  printf '/mesh status\n'
  printf '/mesh beacon\n'
  printf '/tenant status\n'
  printf '/quota status\n'
  printf '/quit beta\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "v0.1-beta demo complete" 180 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "OpenAgentOS v0.1-beta" \
           "\\[console\\] service ready id=10" \
           "\\[fleet\\] platform=" \
           "\\[fleet\\] push ok" \
           "\\[policy\\] deny agent=10 tool=10" \
           "\\[policy\\] probe agent=10 tool=10 rc=-1" \
           "\\[policy\\] deny ok" \
           "\\[remote\\] enable agent=10 port=5557" \
           "\\[mesh\\] beacon device=box-x86-001" \
           "\\[tenant\\] agent=10 session" \
           "\\[quota\\] agent=10 ipc=" \
           "v0.1-beta demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-v0.1-beta] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-v0.1-beta] failed; log=$LOG"
  tail -160 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-v0.1-beta] ok (x86 v0.1-beta GA full stack)"
