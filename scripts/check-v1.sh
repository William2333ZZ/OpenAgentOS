#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-v1-check.img}"
KERNEL="$ROOT/kernel-v1.elf"
LOG="$(mktemp /tmp/agentos-v1-check.XXXXXX)"

cd "$ROOT"
make kernel-v1.elf
dd if=/dev/zero of="$DISK" bs=512 count=128 status=none

qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,id=blk,format=raw,file="$DISK" \
  -device virtio-blk-device,drive=blk \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 60); do
  if rg -q "v1 demo complete" "$LOG" 2>/dev/null; then
    kill "$QPID" 2>/dev/null || true
    wait "$QPID" 2>/dev/null || true
    if rg -q "shutdown" "$LOG"; then
      echo "[check-v1] ok"
      rm -f "$LOG"
      exit 0
    fi
    break
  fi
  if ! kill -0 "$QPID" 2>/dev/null; then
    break
  fi
  sleep 0.25
done

kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true
echo "[check-v1] failed; log=$LOG"
tail -30 "$LOG"
exit 1
