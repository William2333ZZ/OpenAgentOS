#!/usr/bin/env python3
"""Host gateway for OpenAgentOS 0.4.0 remote TCP connect (slirp 10.0.2.2).

Guest `/remote connect` opens TCP to 10.0.2.2:5557 and sends:
  LINK <token>\\n

This gateway replies PONG and logs the session.
"""

from __future__ import annotations

import argparse
import socket
import sys


def handle(conn: socket.socket, addr: tuple[str, int]) -> None:
    data = conn.recv(512)
    text = data.decode("utf-8", errors="replace").strip()
    print(f"[remote-gateway] client {addr[0]}:{addr[1]} data={text!r}", flush=True)
    if "LINK" in text or "PING" in text:
        conn.sendall(b"PONG\n")
    else:
        conn.sendall(b"ERR\n")
    conn.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="OpenAgentOS remote TCP gateway")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5557)
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.listen(4)
    print(f"[remote-gateway] listening on {args.host}:{args.port}", flush=True)

    while True:
        conn, addr = sock.accept()
        try:
            handle(conn, addr)
        except OSError as exc:
            print(f"[remote-gateway] error: {exc}", file=sys.stderr, flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
