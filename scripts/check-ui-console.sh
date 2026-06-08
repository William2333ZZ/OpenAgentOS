#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel-ui.elf"
LOG="$(mktemp /tmp/agentos-ui-console-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-ui-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-ui.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-ui-console] launching QEMU without GPU (UART stdin/stdout) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 1.2
  printf 'h'
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

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
kill "$WRITER" 2>/dev/null || true
wait "$WRITER" 2>/dev/null || true

fail=0
for pat in "no GPU" "display] service ready" "input] service ready" \
           "ui-console]" "display draw ok" "input key ok code=" "ui demo complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-ui-console] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-ui-console] failed; log=$LOG"
  tail -80 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-ui-console] ok (headless: UART display + serial input, no virtio-gpu)"
