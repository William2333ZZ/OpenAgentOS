#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[check-0.7.0] OTA format-2 sign self-test ..."
make -s worker-ota-v1.agent
python3 tools/ota-sign.py --elf worker-ota-v1.agent --out /tmp/agentos-signed.agentpkg \
  --name worker --version 0.7.0 --install /agent/1/w.agent 2>/dev/null || \
python3 tools/ota-sign.py --elf worker-ota-v1.agent --out /tmp/agentos-signed.agentpkg \
  --name worker --version 0.7.0 --install /agent/1/w.agent

python3 - <<'PY'
import hashlib, struct, sys
OTA_HMAC_KEY = b"openagentos-ota-dev-key-v1"
OTA_MANIFEST_SIZE = 128
data = open("/tmp/agentos-signed.agentpkg", "rb").read()
magic, fmt, sig = struct.unpack_from("<III", data, 0)
assert magic == 0x4B504F41
assert fmt == 2
body = bytearray(data[:OTA_MANIFEST_SIZE])
body[96:128] = b"\x00" * 32
body = bytes(body) + data[OTA_MANIFEST_SIZE:]
hmac = hashlib.sha256(OTA_HMAC_KEY + body).digest()
got = data[96:128]
assert hmac == got, "hmac mismatch"
print("[check-0.7.0] format-2 hmac ok")
PY

make kernel-ota.elf
chmod +x scripts/check-ota.sh
./scripts/check-ota.sh

echo "[check-0.7.0] ok (signed OTA + legacy check-ota)"
