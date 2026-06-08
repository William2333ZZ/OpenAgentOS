#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-net-prod.elf"
LOG="$(mktemp /tmp/agentos-net-prod-check.XXXXXX)"
SERVER_PID=""

cleanup() {
  if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

cd "$ROOT"
make kernel-net-prod.elf

echo "[check-net-prod] starting test HTTP server on :8080 ..."
python3 tools/net-test-server.py 8080 >"${LOG}.server" 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "[check-net-prod] test server failed to start"
  cat "${LOG}.server" || true
  exit 1
fi

qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 160); do
  if rg -q "net prod demo complete" "$LOG" 2>/dev/null; then
    kill "$QPID" 2>/dev/null || true
    wait "$QPID" 2>/dev/null || true
    break
  fi
  if ! kill -0 "$QPID" 2>/dev/null; then
    break
  fi
  sleep 0.25
done

kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

fail=0
for pat in "net prod demo complete" "network offline rejected" "network native ok" \
           "network cache fallback ok" "network quota exceeded" \
           "network] backend=native" "network] backend=prod" \
           "network] cache hit" "network] fallback cache ok" \
           "network] quota exceeded" "tool:audit.*agent=7 tool=11" \
           "network] shutdown" "native body: 200:agentos-net-stub" \
           "virtio-net] driver ready" "net] http_get status=200"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-net-prod] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-net-prod] failed; log=$LOG server=${LOG}.server"
  tail -80 "$LOG"
  tail -20 "${LOG}.server" 2>/dev/null || true
  exit 1
fi

rm -f "$LOG" "${LOG}.server"
echo "[check-net-prod] ok (native virtio-net TCP/HTTP + cache fallback + quota + audit)"
