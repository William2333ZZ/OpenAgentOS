#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-wrap-x86.img}"
KERNEL="$ROOT/kernel-x86-wrap.elf"
LOG1="$(mktemp /tmp/agentos-wrap-x86-boot1.XXXXXX)"
LOG2="$(mktemp /tmp/agentos-wrap-x86-boot2.XXXXXX)"

cd "$ROOT"
make kernel-x86-wrap.elf
dd if=/dev/zero of="$DISK" bs=512 count=256 status=none

run_boot() {
  local log="$1"
  qemu-system-x86_64 -machine pc -nographic -kernel "$KERNEL" -display none \
    -drive if=none,id=blk,format=raw,file="$DISK" \
    -device virtio-blk-pci,drive=blk >"$log" 2>&1 &
  echo $!
}

wait_for() {
  local qpid="$1"
  local log="$2"
  local pattern="$3"
  for _ in $(seq 1 120); do
    if rg -q "$pattern" "$log" 2>/dev/null; then
      kill "$qpid" 2>/dev/null || true
      wait "$qpid" 2>/dev/null || true
      return 0
    fi
    if ! kill -0 "$qpid" 2>/dev/null; then
      break
    fi
    sleep 0.25
  done
  kill "$qpid" 2>/dev/null || true
  wait "$qpid" 2>/dev/null || true
  return 1
}

QPID1="$(run_boot "$LOG1")"
if ! wait_for "$QPID1" "$LOG1" "wrap demo complete"; then
  echo "[check-wrap-x86] boot1 failed; log=$LOG1"
  tail -60 "$LOG1"
  exit 1
fi

for pat in "ramfs stress 20 files ok" "tool=10 rc=-1" "persist sync ok" \
           "wrap boot1 complete"; do
  if ! rg -q "$pat" "$LOG1" 2>/dev/null; then
    echo "[check-wrap-x86] boot1 missing: $pat"
    tail -60 "$LOG1"
    exit 1
  fi
done

QPID2="$(run_boot "$LOG2")"
if ! wait_for "$QPID2" "$LOG2" "wrap demo complete"; then
  echo "[check-wrap-x86] boot2 failed; log=$LOG2"
  tail -60 "$LOG2"
  exit 1
fi

for pat in "loading .* files from AOS2 block" "stress file restored ok" \
           "audit persist ok" "wrap boot2 complete"; do
  if ! rg -q "$pat" "$LOG2" 2>/dev/null; then
    echo "[check-wrap-x86] boot2 missing: $pat"
    tail -60 "$LOG2"
    exit 1
  fi
done

rm -f "$LOG1" "$LOG2"
echo "[check-wrap-x86] ok (x86 wrap + persist)"
