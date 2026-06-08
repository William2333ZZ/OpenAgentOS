#!/usr/bin/env python3
"""HTTP gateway on :8443 — guest POSTs here, host forwards to DeepSeek HTTPS."""

import http.server
import os
import sys
import urllib.error
import urllib.request

API_URL = "https://api.deepseek.com/chat/completions"
PORT = 8443


class Gateway(http.server.BaseHTTPRequestHandler):
    def do_POST(self) -> None:
        if self.path != "/chat/completions":
            self.send_error(404)
            return
        auth = self.headers.get("Authorization", "")
        if not auth.lower().startswith("bearer "):
            self.send_error(401)
            return
        clen = int(self.headers.get("Content-Length", "0") or "0")
        body = self.rfile.read(clen) if clen > 0 else b""
        req = urllib.request.Request(
            API_URL,
            data=body,
            headers={
                "Authorization": auth,
                "Content-Type": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = resp.read()
        except urllib.error.HTTPError as e:
            data = e.read()
            self.send_response(e.code)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("[deepseek-net-gw] " + (fmt % args) + "\n")


def main() -> None:
    if not os.environ.get("DEEPSEEK_API_KEY"):
        env_path = os.path.join(os.path.dirname(__file__), "..", ".env")
        if os.path.isfile(env_path):
            for line in open(env_path, encoding="utf-8"):
                line = line.strip()
                if line.startswith("DEEPSEEK_API_KEY="):
                    os.environ["DEEPSEEK_API_KEY"] = line.split("=", 1)[1].strip().strip("'\"")
                    break
    http.server.HTTPServer(("0.0.0.0", PORT), Gateway).serve_forever()


if __name__ == "__main__":
    main()
