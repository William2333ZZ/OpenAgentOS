#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-tools2.elf"
LOG="$(mktemp /tmp/agentos-tools2-check.XXXXXX)"

cd "$ROOT"
make kernel-tools2.elf

qemu-system-riscv64 -machine virt -nographic -bios default \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

for _ in $(seq 1 60); do
  if rg -q "tools2 demo complete" "$LOG" 2>/dev/null; then
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
for pat in "tools2 demo complete" "audit eperm ok" "audit allow ok" \
           "tool=10 rc=-1" "tool=11 rc=-1" "tool=10 rc=1" \
           "[tool:audit]" "[tool:gpio]" "[tool:http]"; do
  count="$(rg -c "$pat" "$LOG" 2>/dev/null || true)"
  if [[ -z "$count" ]] || [[ "$count" -lt 1 ]]; then
    echo "[check-tools2] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-tools2] failed; log=$LOG"
  tail -50 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-tools2] ok (policy EPERM + audit log + gpio/http stub)"
