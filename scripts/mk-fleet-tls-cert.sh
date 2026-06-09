#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="$ROOT/generated/fleet-tls"
mkdir -p "$DIR"
if [[ ! -f "$DIR/server.key" ]]; then
  openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$DIR/server.key" -out "$DIR/server.pem" -days 3650 \
    -subj "/CN=OpenAgentOS-Fleet-Dev" 2>/dev/null
  echo "[mk-fleet-tls-cert] wrote $DIR/server.pem"
else
  echo "[mk-fleet-tls-cert] reuse $DIR/server.pem"
fi
