#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-ui.elf"
LOG="$(mktemp /tmp/agentos-ui-check.XXXXXX)"
MON_SOCK="$(mktemp -u /tmp/agentos-qemu-mon.XXXXXX).sock"

cd "$ROOT"
make kernel-ui.elf

echo "[check-ui] launching QEMU with virtio-gpu + virtio-keyboard ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -device virtio-gpu-device \
  -device virtio-keyboard-device \
  -monitor "unix:${MON_SOCK},server,nowait" \
  -kernel "$KERNEL" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 1
  while kill -0 "$QPID" 2>/dev/null; do
    printf 'sendkey h\n' | nc -U "$MON_SOCK" >/dev/null 2>&1 || true
    sleep 0.15
  done
) &
INJECT_PID=$!

for _ in $(seq 1 120); do
  if rg -q "ui demo complete" "$LOG" 2>/dev/null; then
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
kill "$INJECT_PID" 2>/dev/null || true
wait "$INJECT_PID" 2>/dev/null || true
rm -f "$MON_SOCK"

fail=0
for pat in "virtio-gpu] driver ready" "virtio-input] driver ready" \
           "display] service ready" "input] service ready" \
           "display draw ok" "input key ok code=" "ui demo complete" \
           "input] key down code="; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-ui] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-ui] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-ui] ok (virtio-gpu text + virtio-keyboard input via display/input agents)"
