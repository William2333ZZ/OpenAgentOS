#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-router-faux.elf"
LOG="$(mktemp /tmp/agentos-router-check.XXXXXX)"

cd "$ROOT"
make kernel-router-faux.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 80); do
  if rg -q "router demo complete" "$LOG" 2>/dev/null; then
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
for pat in "router demo complete" "router offline rejected" "router faux ok" \
           "router] backend=faux" "client] answer: 42" "router] shutdown"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-router] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-router] failed; log=$LOG"
  tail -50 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-router] ok (faux via router + offline rejected)"
