#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-llm-net.elf"
LOG="$(mktemp /tmp/agentos-llm-net-run.XXXXXX)"
GWPID=""

cd "$ROOT"

cleanup() {
  if [[ -n "$GWPID" ]] && kill -0 "$GWPID" 2>/dev/null; then
    kill "$GWPID" 2>/dev/null || true
    wait "$GWPID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ ! -f .env ]] && [[ -z "${DEEPSEEK_API_KEY:-}" ]]; then
  echo "[run-llm-net] missing DEEPSEEK_API_KEY (.env or env var required)"
  exit 1
fi

./scripts/mk-deepseek-key.sh
./scripts/mk-deepseek-host.sh
make kernel-llm-net.elf

echo "[run-llm-net] starting host HTTP gateway on :8443 ..."
python3 tools/deepseek-net-gw.py >"${LOG}.gw" 2>&1 &
GWPID=$!
sleep 0.5
if ! kill -0 "$GWPID" 2>/dev/null; then
  echo "[run-llm-net] gateway failed to start"
  cat "${LOG}.gw" || true
  exit 1
fi

echo "[run-llm-net] launching QEMU (Ctrl-A X to quit) ..."
exec qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
  -kernel "$KERNEL"
