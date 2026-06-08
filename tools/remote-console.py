#!/usr/bin/env python3
"""Host-side Remote Console proxy for AgentOS v0.1-beta (QEMU stdio bridge).

Usage:
  python3 tools/remote-console.py --port 5557

Connect with: nc localhost 5557
Lines are forwarded to the attached terminal (when run under QEMU -serial stdio).
This script is a development stub; production uses virtio-net + token auth.
"""

from __future__ import annotations

import argparse
import socket
import sys
import threading


def handle_client(conn: socket.socket, addr: tuple[str, int]) -> None:
    print(f"[remote-console] client {addr[0]}:{addr[1]}", file=sys.stderr)
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
    finally:
        conn.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="AgentOS remote console TCP stub")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5557)
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.listen(1)
    print(f"[remote-console] listening on {args.host}:{args.port}", file=sys.stderr)

    while True:
        conn, addr = sock.accept()
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()


if __name__ == "__main__":
    raise SystemExit(main())
