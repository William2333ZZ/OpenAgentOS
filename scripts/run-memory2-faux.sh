#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-memory2-faux.img}"
KERNEL="$ROOT/kernel-memory2-faux.elf"

cd "$ROOT"
make kernel-memory2-faux.elf

if [[ ! -f "$DISK" ]]; then
  echo "[run-memory2-faux] creating block image $DISK (128 sectors)"
  dd if=/dev/zero of="$DISK" bs=512 count=128 status=none
fi

echo "[run-memory2-faux] disk=$DISK kernel=$KERNEL"
exec qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,id=blk,format=raw,file="$DISK" \
  -device virtio-blk-device,drive=blk \
  -kernel "$KERNEL"
