#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG="$(mktemp /tmp/agentos-smp.XXXXXX)"
KERNEL="$ROOT/kernel-smp.elf"
SMP="${AGENTOS_SMP:-2}"

cd "$ROOT"
make kernel-smp.elf

qemu-system-riscv64 -machine virt -nographic -bios default -smp "$SMP" \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 120); do
  if rg -q "smp demo complete" "$LOG" 2>/dev/null; then
    for _ in $(seq 1 40); do
      if rg -q "shutdown" "$LOG" 2>/dev/null; then
        break
      fi
      sleep 0.1
    done
    sleep 0.3
    kill "$QPID" 2>/dev/null || true
    wait "$QPID" 2>/dev/null || true
    break
  fi
  if ! kill -0 "$QPID" 2>/dev/null; then
    sleep 0.3
    break
  fi
  sleep 0.25
done

for pat in "online harts=2" "worker-a] hart=" "worker-b] hart=" \
           "concurrent workers ok" "smp demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-smp] missing: $pat"
    tail -50 "$LOG"
    exit 1
  fi
done

rm -f "$LOG"
echo "[check-smp] ok ($SMP harts concurrent agents + stress)"
