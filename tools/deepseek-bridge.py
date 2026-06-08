#!/usr/bin/env python3
"""AgentOS DeepSeek bridge: virtio-console socket <-> DeepSeek API."""

import json
import os
import socket
import struct
import sys
import urllib.error
import urllib.request

API_URL = "https://api.deepseek.com/chat/completions"
MODEL = "deepseek-chat"
HOST = "127.0.0.1"
PORT = 5555


def load_api_key() -> str:
    key = os.environ.get("DEEPSEEK_API_KEY", "").strip()
    if not key:
        env_path = os.path.join(os.path.dirname(__file__), "..", ".env")
        if os.path.exists(env_path):
            for line in open(env_path, encoding="utf-8"):
                line = line.strip()
                if line.startswith("DEEPSEEK_API_KEY="):
                    key = line.split("=", 1)[1].strip().strip('"').strip("'")
                    break
    if not key:
        print("Set DEEPSEEK_API_KEY in environment or .env", file=sys.stderr)
        sys.exit(1)
    return key


def read_exact(conn: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed")
        buf += chunk
    return buf


def read_line(conn: socket.socket) -> str:
    buf = b""
    while True:
        chunk = conn.recv(1)
        if not chunk:
            raise ConnectionError("socket closed")
        buf += chunk
        if chunk == b"\n":
            break
    return buf.decode("utf-8", errors="replace").strip()


def deepseek_chat(api_key: str, prompt: str) -> str:
    payload = {
        "model": MODEL,
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 256,
        "temperature": 0,
    }
    req = urllib.request.Request(
        API_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        body = json.loads(resp.read().decode("utf-8"))
    return body["choices"][0]["message"]["content"].strip()


def handle_legacy(conn: socket.socket, api_key: str, header: bytes) -> None:
    if header[:4] != b"LLMQ":
        raise ValueError(f"bad magic: {header[:4]!r}")
    (plen,) = struct.unpack(">I", header[4:8])
    prompt = read_exact(conn, plen).decode("utf-8", errors="replace")
    print(f"[bridge] prompt: {prompt[:80]!r}", flush=True)
    answer = deepseek_chat(api_key, prompt)
    print(f"[bridge] answer: {answer[:80]!r}", flush=True)
    body = answer.encode("utf-8")
    conn.sendall(b"LLMR" + struct.pack(">I", len(body)) + body)


def handle_v2(conn: socket.socket, api_key: str, first_line: str) -> None:
    req = json.loads(first_line)
    if req.get("op") != "query":
        raise ValueError(f"unsupported op: {req.get('op')!r}")
    prompt = req.get("prompt", "")
    print(f"[bridge] v2 prompt: {prompt[:80]!r}", flush=True)

    answer = deepseek_chat(api_key, prompt)
    print(f"[bridge] v2 answer: {answer[:80]!r}", flush=True)

    out = []
    if len(answer) <= 1:
        out.append(json.dumps({"v": 2, "op": "delta", "chunk": answer}, ensure_ascii=False) + "\n")
    else:
        mid = max(1, len(answer) // 2)
        out.append(json.dumps({"v": 2, "op": "delta", "chunk": answer[:mid]}, ensure_ascii=False) + "\n")
        out.append(json.dumps({"v": 2, "op": "delta", "chunk": answer[mid:]}, ensure_ascii=False) + "\n")
    out.append(json.dumps({"v": 2, "op": "end", "text": answer}, ensure_ascii=False) + "\n")
    conn.sendall("".join(out).encode("utf-8"))


def handle_client(conn: socket.socket, api_key: str) -> None:
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
        handle_v2(conn, api_key, (first + rest).decode("utf-8", errors="replace").strip())
        return

    header = first + read_exact(conn, 7)
    handle_legacy(conn, api_key, header)


def main() -> None:
    api_key = load_api_key()
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))
    srv.listen(1)
    print(f"[bridge] listening on {HOST}:{PORT}", flush=True)

    while True:
        conn, addr = srv.accept()
        print(f"[bridge] guest connected from {addr}", flush=True)
        try:
            handle_client(conn, api_key)
        except Exception as exc:  # noqa: BLE001
            print(f"[bridge] error: {exc}", file=sys.stderr, flush=True)
        finally:
            conn.close()


if __name__ == "__main__":
    main()
