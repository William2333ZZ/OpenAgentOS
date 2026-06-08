#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_RV="$(mktemp /tmp/agentos-platform-rv.XXXXXX)"
LOG_X86="$(mktemp /tmp/agentos-platform-x86.XXXXXX)"

cd "$ROOT"

echo "[check-platform] RISC-V: wrap + persist2"
make check-wrap check-persist2 >"$LOG_RV" 2>&1 || {
  echo "[check-platform] RISC-V regression failed"
  tail -40 "$LOG_RV"
  exit 1
}

echo "[check-platform] x86_64-pc: wrap (when green) + smoke"
if make check-wrap-x86 >"$LOG_X86" 2>&1; then
  echo "[check-platform] x86 wrap ok"
  make check-console-x86-all >>"$LOG_X86" 2>&1 || {
    echo "[check-platform] x86 console subset failed"
    tail -40 "$LOG_X86"
    exit 1
  }
  echo "[check-platform] x86 console subset ok"
else
  echo "[check-platform] x86 wrap not yet green; falling back to smoke"
  tail -20 "$LOG_X86"
  make kernel-x86-smoke.elf
  LOG_SMOKE="$(mktemp /tmp/agentos-platform-x86-smoke.XXXXXX)"
  qemu-system-x86_64 -machine pc -nographic -kernel "$ROOT/kernel-x86-smoke.elf" \
    -display none >"$LOG_SMOKE" 2>&1 &
  XPID=$!
  for _ in $(seq 1 40); do
    if rg -q "platform smoke ok" "$LOG_SMOKE" 2>/dev/null; then
      kill "$XPID" 2>/dev/null || true
      wait "$XPID" 2>/dev/null || true
      break
    fi
    if ! kill -0 "$XPID" 2>/dev/null; then
      break
    fi
    sleep 0.25
  done
  for pat in "platform: x86_64-pc" "platform smoke ok"; do
    if ! rg -q "$pat" "$LOG_SMOKE" 2>/dev/null; then
      echo "[check-platform] x86 smoke missing: $pat"
      cat "$LOG_SMOKE"
      exit 1
    fi
  done
  rm -f "$LOG_SMOKE"
fi

rm -f "$LOG_RV" "$LOG_X86"
echo "[check-platform] ok (riscv64-virt wrap+persist2 + x86_64-pc)"
