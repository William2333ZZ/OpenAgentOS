#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-vm3.elf"
LOG="$(mktemp /tmp/agentos-vm3-check.XXXXXX)"

cd "$ROOT"
make kernel-vm3.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 80); do
  if rg -q "vm3 demo complete" "$LOG" 2>/dev/null; then
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
for pat in "vm3 demo complete" "heap demand map ok" "heap COW ok" "heap-ok" \
           "syscall EFAULT ok" "user fault kill agent=" \
           "heap cow alias" "heap cow break" "touch guard page below stack" \
           "bad/stack agents faulted"; do
  count="$(rg -c "$pat" "$LOG" 2>/dev/null || true)"
  if [[ -z "$count" ]] || [[ "$count" -lt 1 ]]; then
    echo "[check-vm3] missing: $pat"
    fail=1
  fi
done

if rg -q "unexpected trap" "$LOG" 2>/dev/null; then
  echo "[check-vm3] kernel panic on unexpected trap"
  fail=1
fi

if rg -q "should not reach here" "$LOG" 2>/dev/null; then
  echo "[check-vm3] bad worker survived invalid access"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "[check-vm3] failed; log=$LOG"
  tail -50 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-vm3] ok (demand heap + EFAULT + fault isolation)"
