#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

make -q kernel.elf 2>/dev/null || make

exec qemu-system-riscv64 \
    -machine virt \
    -nographic \
    -bios default \
    -kernel kernel.elf
