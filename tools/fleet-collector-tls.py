#!/usr/bin/env python3
"""HTTPS Fleet collector for OpenAgentOS 0.6.1+ (dev self-signed cert)."""

from __future__ import annotations

import argparse
import ssl
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path


class FleetTlsHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: object) -> None:
        print(f"[fleet-collector-tls] {self.address_string()} - {fmt % args}", flush=True)

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b""
        print(f"[fleet-collector-tls] ingest tls path={self.path} bytes={len(body)}", flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"ok\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8443)
    parser.add_argument(
        "--cert",
        default=str(Path(__file__).resolve().parents[1] / "generated/fleet-tls/server.pem"),
    )
    parser.add_argument(
        "--key",
        default=str(Path(__file__).resolve().parents[1] / "generated/fleet-tls/server.key"),
    )
    args = parser.parse_args()

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.load_cert_chain(args.cert, args.key)
    server = HTTPServer((args.host, args.port), FleetTlsHandler)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    print(f"[fleet-collector-tls] https://{args.host}:{args.port}/ingest", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
