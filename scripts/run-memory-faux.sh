#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-memory-faux.img}"
KERNEL="$ROOT/kernel-memory-faux.elf"

cd "$ROOT"
make kernel-memory-faux.elf

if [[ ! -f "$DISK" ]]; then
  echo "[run-memory-faux] creating block image $DISK (128 sectors)"
  dd if=/dev/zero of="$DISK" bs=512 count=128 status=none
fi

echo "[run-memory-faux] disk=$DISK kernel=$KERNEL (LLM_FAUX, no bridge)"
echo "[run-memory-faux] tip: rerun with same AGENTOS_DISK to verify cross-boot session"
exec qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,id=blk,format=raw,file="$DISK" \
  -kernel "$KERNEL"
