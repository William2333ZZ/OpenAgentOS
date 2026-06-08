#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/check-console-common.sh
source "$ROOT/scripts/check-console-common.sh"

KERNEL="$ROOT/kernel-console-session.elf"
LOG="$(mktemp /tmp/agentos-console-session-check.XXXXXX)"
FIFO_IN="$(mktemp -u /tmp/agentos-console-session-stdin.XXXXXX).fifo"

cd "$ROOT"
make kernel-console-session.elf

cleanup() {
  rm -f "$FIFO_IN"
}
trap cleanup EXIT

mkfifo "$FIFO_IN"

echo "[check-console-session] launching QEMU without GPU (session REPL) ..."
qemu-system-riscv64 -machine virt -nographic -bios default \
  -global virtio-mmio.force-legacy=false \
  -kernel "$KERNEL" <"$FIFO_IN" >"$LOG" 2>&1 &
QPID=$!

(
  sleep 2
  cat <<'EOF'
/session append user:plan edge demo
/session append assistant: noted
/session append user: turn2
/session append assistant: ready
/llm Summarize in one word.
/session tail
/compact 2
/quit session
EOF
  while kill -0 "$QPID" 2>/dev/null; do
    sleep 0.2
  done
) >"$FIFO_IN" &
WRITER=$!

RC=0
wait_console_qemu "$LOG" "$QPID" "console demo session complete" 120 || RC=1
stop_console_qemu "$QPID" "$WRITER"

fail=$RC
for pat in "no GPU" "service ready id=10" "append len=20" \
           "\\[session\\] summary:" "console demo session complete"; do
  if ! rg -q "$pat" "$LOG" 2>/dev/null; then
    echo "[check-console-session] missing: $pat"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "[check-console-session] failed; log=$LOG"
  tail -100 "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "[check-console-session] ok (session tail + compact + UART REPL)"
