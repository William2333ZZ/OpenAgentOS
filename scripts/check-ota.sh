#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-ota-check.img}"
KERNEL_OLD="$ROOT/kernel-ota-old.elf"
KERNEL_NEW="$ROOT/kernel-ota.elf"
LOG1="$(mktemp /tmp/agentos-ota-boot1.XXXXXX)"
LOG2="$(mktemp /tmp/agentos-ota-boot2.XXXXXX)"

cd "$ROOT"
make kernel-ota-old.elf kernel-ota.elf
dd if=/dev/zero of="$DISK" bs=512 count=256 status=none

run_boot() {
  local kernel="$1"
  local log="$2"
  qemu-system-riscv64 -machine virt -nographic -bios default \
    -global virtio-mmio.force-legacy=false \
    -drive if=none,id=blk,format=raw,file="$DISK" \
    -device virtio-blk-device,drive=blk \
    -kernel "$kernel" >"$log" 2>&1 &
  echo $!
}

wait_for() {
  local qpid="$1"
  local log="$2"
  local pattern="$3"
  for _ in $(seq 1 80); do
    if rg -q "$pattern" "$log" 2>/dev/null; then
      for _w in $(seq 1 20); do
        if rg -q "shutdown" "$log" 2>/dev/null; then
          break
        fi
        sleep 0.1
      done
      sleep 0.2
      kill "$qpid" 2>/dev/null || true
      wait "$qpid" 2>/dev/null || true
      return 0
    fi
    if ! kill -0 "$qpid" 2>/dev/null; then
      sleep 0.2
      break
    fi
    sleep 0.25
  done
  kill "$qpid" 2>/dev/null || true
  wait "$qpid" 2>/dev/null || true
  return 1
}

QPID1="$(run_boot "$KERNEL_OLD" "$LOG1")"
if ! wait_for "$QPID1" "$LOG1" "ota boot1 complete"; then
  echo "[check-ota] boot1 (old kernel) failed; log=$LOG1"
  tail -50 "$LOG1"
  exit 1
fi

for pat in "ota kernel gen=1" "worker result: sum=42" "staging worker-v2 pkg" \
           "sync complete AOS2"; do
  if ! rg -q "$pat" "$LOG1" 2>/dev/null; then
    echo "[check-ota] boot1 missing: $pat"
    tail -50 "$LOG1"
    exit 1
  fi
done

QPID2="$(run_boot "$KERNEL_NEW" "$LOG2")"
if ! wait_for "$QPID2" "$LOG2" "ota demo complete"; then
  echo "[check-ota] boot2 (new kernel) failed; log=$LOG2"
  tail -50 "$LOG2"
  exit 1
fi

for pat in "ota kernel gen=2" "marker restored: ota-seed-v1" "ota verify ok" \
           "ota apply ok" "worker result: ver=2 sum=84" "ramfs marker still ok" \
           "loading .* files from AOS2 block"; do
  if ! rg -q "$pat" "$LOG2" 2>/dev/null; then
    echo "[check-ota] boot2 missing: $pat"
    tail -50 "$LOG2"
    exit 1
  fi
done

rm -f "$LOG1" "$LOG2"
echo "[check-ota] ok (old kernel seed -> OTA apply -> v2 agent + persist)"
