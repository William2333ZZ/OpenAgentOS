#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-storage2.elf"
LOG="$(mktemp /tmp/agentos-storage2-check.XXXXXX)"

cd "$ROOT"
make kernel-storage2.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 80); do
  if rg -q "storage2 demo complete" "$LOG" 2>/dev/null; then
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
for pat in "storage2 demo complete" "readback ok" "workerA-data" "workerB-data" \
           "read req=" "write req="; do
  count="$(rg -c "$pat" "$LOG" 2>/dev/null || true)"
  if [[ -z "$count" ]] || [[ "$count" -lt 1 ]]; then
    echo "[check-storage2] missing: $pat"
    fail=1
  fi
done

if rg -q "readback mismatch" "$LOG" 2>/dev/null; then
  echo "[check-storage2] readback mismatch detected"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "[check-storage2] failed; log=$LOG"
  tail -40 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-storage2] ok (concurrent workers, req_id RPC)"
