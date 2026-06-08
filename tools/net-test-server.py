#!/usr/bin/env python3
"""Minimal HTTP server for AgentOS native net check (QEMU slirp 10.0.2.2)."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import sys

PORT = 8080


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[net-test-server] {self.address_string()} {fmt % args}", flush=True)

    def do_GET(self):
        if "reject" in self.path:
            self.send_response(403)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"err:rejected")
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(b"200:agentos-net-stub")


def main() -> None:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else PORT
    srv = HTTPServer(("0.0.0.0", port), Handler)
    print(f"[net-test-server] listening on 0.0.0.0:{port}", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
