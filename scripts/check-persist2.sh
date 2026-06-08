#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DISK="${AGENTOS_DISK:-/tmp/agentos-persist2-check.img}"
KERNEL="$ROOT/kernel-persist2.elf"
LOG1="$(mktemp /tmp/agentos-persist2-boot1.XXXXXX)"
LOG2="$(mktemp /tmp/agentos-persist2-boot2.XXXXXX)"

cd "$ROOT"
make kernel-persist2.elf
python3 "$ROOT/scripts/seed-aos1-disk.py" "$DISK" "aos1-seed"

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
if ! wait_for "$QPID1" "$LOG1" "persist2 demo complete"; then
  echo "[check-persist2] boot1 failed; log=$LOG1"
  tail -50 "$LOG1"
  exit 1
fi

for pat in "loading .* files from AOS1 block" "migrate AOS1 -> AOS2" \
           "marker: aos1-seed" "sync complete AOS2" "aos2 sync ok"; do
  if ! rg -q "$pat" "$LOG1" 2>/dev/null; then
    echo "[check-persist2] boot1 missing: $pat"
    tail -50 "$LOG1"
    exit 1
  fi
done

QPID2="$(run_boot "$LOG2")"
if ! wait_for "$QPID2" "$LOG2" "persist2 demo complete"; then
  echo "[check-persist2] boot2 failed; log=$LOG2"
  tail -50 "$LOG2"
  exit 1
fi

for pat in "loading .* files from AOS2 block" "marker: aos1-seed" \
           "aos2 sync ok"; do
  if ! rg -q "$pat" "$LOG2" 2>/dev/null; then
    echo "[check-persist2] boot2 missing: $pat"
    tail -50 "$LOG2"
    exit 1
  fi
done

if rg -q "migrate AOS1 -> AOS2" "$LOG2" 2>/dev/null; then
  echo "[check-persist2] boot2 should not migrate again"
  tail -50 "$LOG2"
  exit 1
fi

rm -f "$LOG1" "$LOG2"
echo "[check-persist2] ok (AOS1 seed -> migrate -> AOS2 reload)"
