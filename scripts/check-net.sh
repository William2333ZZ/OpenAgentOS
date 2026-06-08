#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-net-faux.elf"
LOG="$(mktemp /tmp/agentos-net-check.XXXXXX)"

cd "$ROOT"
make kernel-net-faux.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 80); do
  if rg -q "net demo complete" "$LOG" 2>/dev/null; then
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
for pat in "net demo complete" "network offline rejected" "network faux ok" \
           "network] backend=faux" "client] body: 200:agentos-net-stub" \
           "tool:audit.*agent=7 tool=11" "network] shutdown"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-net] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-net] failed; log=$LOG"
  tail -50 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-net] ok (faux via network service + offline rejected + audit)"
