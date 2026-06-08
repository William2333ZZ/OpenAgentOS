#!/usr/bin/env python3
"""Minimal Fleet telemetry collector for OpenAgentOS development.

Usage:
  python3 tools/fleet-collector.py --port 8765

Fleet probe from console:
  /fleet probe http://127.0.0.1:8765/ingest

With VirtIO-net (v0.3+), guest POSTs JSON from /fleet push to this endpoint.
"""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer


class FleetHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: object) -> None:
        print(f"[fleet-collector] {self.address_string()} - {fmt % args}", flush=True)

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b""
        print(f"[fleet-collector] ingest path={self.path} bytes={len(body)}", flush=True)
        if body:
            print(body.decode("utf-8", errors="replace"), flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"ok\n")

    def do_GET(self) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"OpenAgentOS fleet-collector ready\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="OpenAgentOS fleet collector stub")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    server = HTTPServer((args.host, args.port), FleetHandler)
    print(f"[fleet-collector] listening on http://{args.host}:{args.port}/", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
