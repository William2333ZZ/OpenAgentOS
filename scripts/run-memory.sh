#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-memory.img}"
KERNEL="$ROOT/kernel-memory.elf"

cd "$ROOT"
make kernel-memory.elf

if [[ ! -f .env ]] && [[ -z "${DEEPSEEK_API_KEY:-}" ]]; then
    echo "Missing DEEPSEEK_API_KEY. Copy .env.example to .env or use make run-memory-faux." >&2
    exit 1
fi

if [[ ! -f "$DISK" ]]; then
  echo "[run-memory] creating block image $DISK (128 sectors)"
  dd if=/dev/zero of="$DISK" bs=512 count=128 status=none
fi

BRIDGE_PID=""
cleanup() {
    if [[ -n "$BRIDGE_PID" ]] && kill -0 "$BRIDGE_PID" 2>/dev/null; then
        kill "$BRIDGE_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "[run-memory] starting DeepSeek bridge on :5555 ..."
python3 tools/deepseek-bridge.py &
BRIDGE_PID=$!
sleep 0.8

echo "[run-memory] disk=$DISK kernel=$KERNEL"
echo "[run-memory] tip: rerun with same AGENTOS_DISK to verify cross-boot session"
exec qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,id=blk,format=raw,file="$DISK" \
  -device virtio-blk-device,drive=blk \
  -chardev socket,id=llm,host=127.0.0.1,port=5555 \
  -device virtio-serial-device \
  -device virtconsole,chardev=llm \
  -kernel "$KERNEL"
