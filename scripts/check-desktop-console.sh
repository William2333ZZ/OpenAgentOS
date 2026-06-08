#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-desktop.elf"
LOG="$(mktemp /tmp/agentos-desktop-console-check.XXXXXX)"

cd "$ROOT"
make kernel-desktop.elf worker.agent user/worker_elf.inc

echo "[check-desktop-console] launching QEMU without GPU ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "desktop console fallback" 120 || RC=1
stop_console_qemu "$QPID"

fail=$RC
for pat in "no GPU" "use Console REPL" "desktop console fallback"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-desktop-console] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-desktop-console] failed; log=$LOG"
  tail -80 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-desktop-console] ok (no GPU → shell fallback to Console REPL)"
