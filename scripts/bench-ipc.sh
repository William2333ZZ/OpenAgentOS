#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-bench-ipc.elf"
LOG="$(mktemp /tmp/agentos-bench-ipc.XXXXXX)"

cd "$ROOT"
make kernel-bench-ipc.elf

echo "[bench-ipc] running ${BENCH_ROUNDS:-32} round ping-pong ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 120); do
  if rg -q "\\[bench-ipc\\] complete" "$LOG" 2>/dev/null; then
    break
  fi
  if ! kill -0 "$QPID" 2>/dev/null; then
    break
  fi
  sleep 0.25
done

kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

if ! rg -q "\\[bench-ipc\\] rounds=" "$LOG" 2>/dev/null; then
  echo "[bench-ipc] failed; log=$LOG"
  tail -50 "$LOG"
  exit 1
fi

rg "\\[bench-ipc\\]" "$LOG" || true
rm -f "$LOG"
echo "[bench-ipc] ok"
