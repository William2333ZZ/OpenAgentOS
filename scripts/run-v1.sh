#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-v1.img}"
KERNEL="$ROOT/kernel-v1.elf"

cd "$ROOT"
make kernel-v1.elf

if [[ ! -f "$DISK" ]]; then
  echo "[run-v1] creating block image $DISK (128 sectors)"
  dd if=/dev/zero of="$DISK" bs=512 count=128 status=none
fi

echo "[run-v1] disk=$DISK kernel=$KERNEL"
echo "[run-v1] tip: run again with same AGENTOS_DISK to verify cross-boot persist"
exec qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,id=blk,format=raw,file="$DISK" \
  -device virtio-blk-device,drive=blk \
  -kernel "$KERNEL"
