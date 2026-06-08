#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-edge-check.img}"
KERNEL="$ROOT/kernel-edge-faux.elf"
LOG1="$(mktemp /tmp/agentos-edge-boot1.XXXXXX)"
LOG2="$(mktemp /tmp/agentos-edge-boot2.XXXXXX)"

cd "$ROOT"
make kernel-edge-faux.elf
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
if ! wait_for "$QPID1" "$LOG1" "edge demo complete"; then
  echo "[check-edge] boot1 failed; log=$LOG1"
  tail -40 "$LOG1"
  exit 1
fi
for pat in "explanation: High temperature" "persist sync ok" "rules] alert: peak=91" \
           "router] backend=faux" "querying router"; do
  if ! rg -q "$pat" "$LOG1" 2>/dev/null; then
    echo "[check-edge] boot1 missing: $pat"
    tail -40 "$LOG1"
    exit 1
  fi
done

QPID2="$(run_boot "$LOG2")"
if ! wait_for "$QPID2" "$LOG2" "edge demo complete"; then
  echo "[check-edge] boot2 failed; log=$LOG2"
  tail -40 "$LOG2"
  exit 1
fi
for pat in "loading .* files from AOS2 block" "edge restore ok" \
           "restored samples: 42,55,91" "restored llm_last: High temperature"; do
  if ! rg -q "$pat" "$LOG2" 2>/dev/null; then
    echo "[check-edge] boot2 missing: $pat"
    tail -40 "$LOG2"
    exit 1
  fi
done

rm -f "$LOG1" "$LOG2"
echo "[check-edge] ok (boot1 pipeline+sync, boot2 persist+restore)"
