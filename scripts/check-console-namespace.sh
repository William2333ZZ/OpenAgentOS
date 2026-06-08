#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-namespace.elf"
LOG="$(mktemp /tmp/agentos-console-namespace-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-namespace-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-namespace.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-namespace] launching QEMU (namespace mount) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/namespace write console-ns-secret\n'
  printf '/namespace status\n'
  printf '/namespace probe 1\n'
  printf '/quit namespace\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo namespace complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" \
           "\\[namespace\\] mount view /ns/agent/<id>/home quota=256" \
           "\\[namespace\\] agent=10 write secret" \
           "\\[namespace\\] agent=10 root=/ns/agent/10/home" \
           "\\[namespace\\] isolation ok" \
           "console demo namespace complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-namespace] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-namespace] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-namespace] ok (namespace mount + cross-agent probe)"
