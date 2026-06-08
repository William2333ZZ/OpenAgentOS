#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-llm-net.elf"
LOG="$(mktemp /tmp/agentos-llm-net-check.XXXXXX)"
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
  echo "[check-llm-net] missing DEEPSEEK_API_KEY (.env or env var required for live API test)"
  exit 1
fi

./scripts/mk-deepseek-key.sh
./scripts/mk-deepseek-host.sh
make kernel-llm-net.elf

echo "[check-llm-net] starting host HTTP gateway on :8443 (DeepSeek TLS offload) ..."
chmod +x tools/deepseek-net-gw.py
python3 tools/deepseek-net-gw.py >"${LOG}.gw" 2>&1 &
GWPID=$!
sleep 0.5
if ! kill -0 "$GWPID" 2>/dev/null; then
  echo "[check-llm-net] gateway failed to start"
  cat "${LOG}.gw" || true
  exit 1
fi

echo "[check-llm-net] launching QEMU with virtio-net (real DeepSeek deepseek-chat) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 360); do
  if rg -q "pipeline complete" "$LOG" 2>/dev/null; then
    kill "$QPID" 2>/dev/null || true
    wait "$QPID" 2>/dev/null || true
    break
  fi
  if ! kill -0 "$QPID" 2>/dev/null; then
    break
  fi
  sleep 0.5
done

kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

fail=0
for pat in "virtio-net] driver ready" "deepseek] POST" "deepseek] answer" \
           "llm-worker] answer:" "pipeline complete, llm said: 42" \
           "http_post status=200"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-llm-net] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-llm-net] failed; log=$LOG gw=${LOG}.gw"
  tail -100 "$LOG"
  tail -20 "${LOG}.gw" 2>/dev/null || true
  exit 1
fi

rm -f "$LOG" "${LOG}.gw"
echo "[check-llm-net] ok (native VirtIO-net + real DeepSeek deepseek-chat via host gw fallback)"
