#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-catalog.elf"
LOG="$(mktemp /tmp/agentos-catalog-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-catalog-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-catalog.elf worker-ota-v1.agent worker-ota-v2.agentpkg \
  user/worker_ota_v1_elf.inc user/worker_ota_v2_pkg.inc

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-catalog] launching QEMU (package catalog) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  printf '/catalog list\n'
  sleep 1
  printf '/load /agent/1/worker.agent worker\n'
  sleep 2
  printf '/catalog install worker\n'
  sleep 2
  printf '/catalog rollback worker\n'
  sleep 2
  printf '/quit catalog\n'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "catalog demo complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "\\[console\\] service ready id=10" "\\[catalog\\] list ok count=1" \
           "worker 2.0.0" "worker result: sum=42" "\\[catalog\\] install ok" \
           "\\[ota\\] applied" "\\[catalog\\] rollback ok" "catalog demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-catalog] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-catalog] failed; log=$LOG"
  tail -120 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-catalog] ok (list + install v2 + rollback v1)"
