#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-desktop.elf"
LOG="$(mktemp /tmp/agentos-desktop-check.XXXXXX)"
MON_SOCK="$(mktemp -u /tmp/agentos-desktop-mon.XXXXXX).sock"

cd "$ROOT"
make kernel-desktop.elf worker.agent user/worker_elf.inc

cleanup() {
  rm -f "$MON_SOCK"
}
trap cleanup EXIT

echo "[check-desktop] launching QEMU with virtio-gpu + keyboard ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -device virtio-gpu-device \
  -device virtio-keyboard-device \
  -monitor "unix:${MON_SOCK},server,nowait" \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 1.5
  for _ in $(seq 1 80); do
    if rg -q "desktop launcher ready" "$LOG" 2>/dev/null; then
      break
    fi
    sleep 0.15
  done
  for _ in $(seq 1 40); do
    printf 'sendkey 1\n' | nc -U "$MON_SOCK" >/dev/null 2>&1 || true
    sleep 0.15
  done
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "desktop demo complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "virtio-gpu] driver ready" "\\[shell\\] service ready id=11" \
           "desktop launcher ready" "\\[worker\\] sum=42" "\\[desktop\\] load ok" \
           "desktop demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-desktop] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-desktop] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-desktop] ok (GPU shell launcher + worker sum=42)"
