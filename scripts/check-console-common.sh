# shellcheck shell=bash
# Shared helpers for Console REPL acceptance scripts (source, do not execute).

# Poll QEMU log: success pattern, console crash, or timeout (~30s default).
# Usage: wait_console_qemu LOG QPID SUCCESS_REGEX [TICKS]
# Returns 0 on success, 1 on failure/timeout.
wait_console_qemu() {
  local log="$1"
  local qpid="$2"
  local success="$3"
  local ticks="${4:-120}"
  local i

  for ((i = 0; i < ticks; i++)); do
    if rg -q "$success" "$log" 2>/dev/null; then
      return 0
    fi
    if console_qemu_failed "$log"; then
      return 1
    fi
    if ! kill -0 "$qpid" 2>/dev/null; then
      return 1
    fi
    sleep 0.25
  done
  echo "[check-console] timeout (${ticks}x0.25s) waiting for: $success"
  return 1
}

# True when console-agent (id=10) crashed or kernel halted agents unexpectedly.
console_qemu_failed() {
  local log="$1"
  if rg -q "page fault agent=10|user fault kill agent=10" "$log" 2>/dev/null; then
    echo "[check-console] console agent crashed (page fault)"
    return 0
  fi
  if rg -q "agent_exit id=10 code=[1-9]" "$log" 2>/dev/null; then
    echo "[check-console] console agent exited with error"
    return 0
  fi
  if rg -q "no runnable agents, halting" "$log" 2>/dev/null; then
    echo "[check-console] kernel halted (no runnable agents)"
    return 0
  fi
  return 1
}

stop_console_qemu() {
  local qpid="$1"
  local writer="${2:-}"
  kill "$qpid" 2>/dev/null || true
  wait "$qpid" 2>/dev/null || true
  if [[ -n "$writer" ]]; then
    kill "$writer" 2>/dev/null || true
    wait "$writer" 2>/dev/null || true
  fi
}

# Poll log for a milestone (e.g. load ok) with fail-fast. ~20s default.
# Usage: wait_console_log LOG QPID PATTERN [TRIES]
wait_console_log() {
  local log="$1"
  local qpid="$2"
  local pattern="$3"
  local tries="${4:-100}"

  for _ in $(seq 1 "$tries"); do
    if rg -q "$pattern" "$log" 2>/dev/null; then
      return 0
    fi
    if console_qemu_failed "$log"; then
      return 1
    fi
    if ! kill -0 "$qpid" 2>/dev/null; then
      return 1
    fi
    sleep 0.2
  done
  echo "[check-console] timeout waiting for log: $pattern"
  return 1
}
