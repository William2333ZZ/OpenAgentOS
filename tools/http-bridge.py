#!/usr/bin/env python3
"""AgentOS HTTP bridge: virtio-console socket <-> stub HTTP responses."""

import json
import socket
import struct
import sys

HOST = "127.0.0.1"
PORT = 5556


def read_exact(conn: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed")
        buf += chunk
    return buf


def handle_httpq(conn: socket.socket, header: bytes) -> None:
    if header[:4] != b"HTTP":
        raise ValueError(f"bad magic: {header[:4]!r}")
    (ulen,) = struct.unpack(">I", header[4:8])
    url = read_exact(conn, ulen).decode("utf-8", errors="replace")
    print(f"[http-bridge] GET {url!r}", flush=True)

    if "reject" in url:
        body = "err:rejected"
        status = 403
    else:
        body = "200:agentos-net-stub"
        status = 200

    payload = body.encode("utf-8")
    conn.sendall(b"HTTP" + struct.pack(">I", len(payload)) + payload)
    print(f"[http-bridge] status={status} body={body!r}", flush=True)


def handle_json(conn: socket.socket, first_line: str) -> None:
    req = json.loads(first_line)
    if req.get("op") != "get":
        raise ValueError(f"unsupported op: {req.get('op')!r}")
    url = req.get("url", "")
    print(f"[http-bridge] json GET {url!r}", flush=True)

    if "reject" in url:
        out = {"v": 1, "op": "err", "status": 403, "body": "err:rejected"}
    else:
        out = {"v": 1, "op": "ok", "status": 200, "body": "200:agentos-net-stub"}
    conn.sendall((json.dumps(out, ensure_ascii=False) + "\n").encode("utf-8"))


def handle_client(conn: socket.socket) -> None:
    first = read_exact(conn, 1)
    if first == b"{":
        rest = b""
        while True:
            chunk = conn.recv(1)
            if not chunk:
                raise ConnectionError("socket closed")
            rest += chunk
            if chunk == b"\n":
                break
        handle_json(conn, (first + rest).decode("utf-8", errors="replace").strip())
        return

    header = first + read_exact(conn, 7)
    if header[:4] == b"HTTP":
        handle_httpq(conn, header)
        return
    raise ValueError(f"unknown protocol: {header[:4]!r}")


def main() -> None:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))
    srv.listen(1)
    print(f"[http-bridge] listening on {HOST}:{PORT}", flush=True)

    while True:
        conn, addr = srv.accept()
        print(f"[http-bridge] guest connected from {addr}", flush=True)
        try:
            handle_client(conn)
        except Exception as exc:  # noqa: BLE001
            print(f"[http-bridge] error: {exc}", file=sys.stderr, flush=True)
        finally:
            conn.close()


if __name__ == "__main__":
    main()
