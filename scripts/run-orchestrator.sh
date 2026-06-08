#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

make -q kernel-orchestrator.elf 2>/dev/null || make kernel-orchestrator.elf

if [[ ! -f .env ]] && [[ -z "${DEEPSEEK_API_KEY:-}" ]]; then
    echo "Missing DEEPSEEK_API_KEY. Copy .env.example to .env or use make run-orchestrator-faux." >&2
    exit 1
fi

BRIDGE_PID=""
cleanup() {
    if [[ -n "$BRIDGE_PID" ]] && kill -0 "$BRIDGE_PID" 2>/dev/null; then
        kill "$BRIDGE_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "[run-orchestrator] starting DeepSeek bridge on :5555 ..."
python3 tools/deepseek-bridge.py &
BRIDGE_PID=$!
sleep 0.8

echo "[run-orchestrator] launching QEMU ..."
exec qemu-system-riscv64 \
    -machine virt \
    -nographic \
    -bios default \
    -kernel kernel-orchestrator.elf \
    -global virtio-mmio.force-legacy=false \
    -chardev socket,id=llm,host=127.0.0.1,port=5555 \
    -device virtio-serial-device \
    -device virtconsole,chardev=llm
