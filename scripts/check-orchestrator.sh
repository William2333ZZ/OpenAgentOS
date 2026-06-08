#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-orchestrator-faux.elf"
LOG="$(mktemp /tmp/agentos-orch-check.XXXXXX)"

cd "$ROOT"
make kernel-orchestrator-faux.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 120); do
  if rg -q "orchestrator complete" "$LOG" 2>/dev/null; then
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
for pat in "orchestrator complete" "\\[worker\\] sum=42" "\\[planner\\] llm summary:" "finished: orchestrator-complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-orchestrator] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-orchestrator] failed; log=$LOG"
  tail -40 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-orchestrator] ok (5-agent pipeline)"
