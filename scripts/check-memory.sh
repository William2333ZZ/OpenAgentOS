#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-memory-check.img}"
KERNEL="$ROOT/kernel-memory-faux.elf"
LOG1="$(mktemp /tmp/agentos-memory-boot1.XXXXXX)"
LOG2="$(mktemp /tmp/agentos-memory-boot2.XXXXXX)"

cd "$ROOT"
make kernel-memory-faux.elf
dd if=/dev/zero of="$DISK" bs=512 count=128 status=none

run_boot() {
  local log="$1"
  qemu-system-riscv64 -machine virt -nographic -bios default \
    -global virtio-mmio.force-legacy=false \
    -drive if=none,id=blk,format=raw,file="$DISK" \
    -device virtio-blk-device,drive=blk \
    -kernel "$KERNEL" >"$log" 2>&1 &
  echo $!
}

wait_for() {
  local qpid="$1"
  local log="$2"
  local pattern="$3"
  for _ in $(seq 1 80); do
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
if ! wait_for "$QPID1" "$LOG1" "memory demo complete"; then
  echo "[check-memory] boot1 failed; log=$LOG1"
  tail -40 "$LOG1"
  exit 1
fi
if ! rg -q "llm answer:" "$LOG1"; then
  echo "[check-memory] boot1 missing llm answer; log=$LOG1"
  tail -40 "$LOG1"
  exit 1
fi
if ! rg -q "persist sync ok" "$LOG1"; then
  echo "[check-memory] boot1 missing persist sync; log=$LOG1"
  tail -40 "$LOG1"
  exit 1
fi

QPID2="$(run_boot "$LOG2")"
if ! wait_for "$QPID2" "$LOG2" "memory demo complete"; then
  echo "[check-memory] boot2 failed; log=$LOG2"
  tail -40 "$LOG2"
  exit 1
fi
if ! rg -q "loading .* files from AOS2 block" "$LOG2"; then
  echo "[check-memory] boot2 missing persist load; log=$LOG2"
  tail -40 "$LOG2"
  exit 1
fi
if ! rg -q "restored prior session tail: assistant:" "$LOG2"; then
  echo "[check-memory] boot2 missing restored session; log=$LOG2"
  tail -40 "$LOG2"
  exit 1
fi

rm -f "$LOG1" "$LOG2"
echo "[check-memory] ok (boot1 llm+sync, boot2 persist+session)"
