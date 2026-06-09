#!/usr/bin/env python3
"""Sign OpenAgentOS OTA packages (format 2 HMAC-SHA256)."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

OTA_PKG_MAGIC = 0x4B504F41
OTA_FMT_SIGNED = 2
OTA_MANIFEST_SIZE = 128
OTA_HMAC_KEY = b"openagentos-ota-dev-key-v1"


def sign_payload(payload: bytes) -> bytes:
    hmac = hashlib.sha256(OTA_HMAC_KEY + payload).digest()
    return hmac


def build_manifest(name: str, version: str, channel: str, elf_offset: int, elf_size: int,
                   install_path: str, hmac: bytes) -> bytes:
    buf = bytearray(OTA_MANIFEST_SIZE)
    struct.pack_into("<III", buf, 0, OTA_PKG_MAGIC, OTA_FMT_SIGNED, 0)
    nb = name.encode()[:31]
    buf[12:12 + len(nb)] = nb
    vb = version.encode()[:15]
    buf[44:44 + len(vb)] = vb
    cb = channel.encode()[:7]
    buf[60:60 + len(cb)] = cb
    struct.pack_into("<III", buf, 68, 0, elf_offset, elf_size)
    pb = install_path.encode()[:15]
    buf[80:80 + len(pb)] = pb
    buf[96:128] = hmac
    return bytes(buf)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, help="worker ELF to package")
    parser.add_argument("--out", required=True)
    parser.add_argument("--name", default="worker")
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--channel", default="stable")
    parser.add_argument("--install", default="/agent/1/worker.agent")
    args = parser.parse_args()

    elf = Path(args.elf).read_bytes()
    pad = b"\x00" * (OTA_MANIFEST_SIZE - len(elf) % OTA_MANIFEST_SIZE)
    if len(pad) == OTA_MANIFEST_SIZE:
        pad = b""
    elf_offset = OTA_MANIFEST_SIZE
    manifest = build_manifest(args.name, args.version, args.channel, elf_offset, len(elf),
                              args.install, b"\x00" * 32)
    body = manifest + pad + elf
    hmac = sign_payload(body)
    manifest = build_manifest(args.name, args.version, args.channel, elf_offset, len(elf),
                              args.install, hmac)
    pkg = manifest + pad + elf
    Path(args.out).write_bytes(pkg)
    print(f"[ota-sign] wrote {args.out} bytes={len(pkg)} hmac={hmac.hex()[:16]}...")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
