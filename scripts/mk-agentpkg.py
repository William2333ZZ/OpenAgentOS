#!/usr/bin/env python3
"""Build a signed AgentOS OTA package (.agentpkg) from a loadable ELF."""

import struct
import sys
import zlib
from pathlib import Path

OTA_PKG_MAGIC = 0x4B504F41  # 'AOPK'
OTA_FMT_VERSION = 1
OTA_MANIFEST_SIZE = 128


def crc32_bytes(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def package_sig(manifest_zero: bytes, elf: bytes) -> int:
    if len(manifest_zero) != OTA_MANIFEST_SIZE:
        raise ValueError("manifest size mismatch")
    return zlib.crc32(manifest_zero + elf) & 0xFFFFFFFF


def build_manifest(name: str, version: str, channel: str, caps: int,
                   install_path: str, elf_size: int, sig: int) -> bytes:
    raw = bytearray(OTA_MANIFEST_SIZE)
    struct.pack_into("<III", raw, 0, OTA_PKG_MAGIC, OTA_FMT_VERSION, sig)
    name_b = name.encode("ascii")[:31]
    ver_b = version.encode("ascii")[:15]
    chan_b = channel.encode("ascii")[:7]
    path_b = install_path.encode("ascii")[:47]
    raw[12:12 + len(name_b)] = name_b
    raw[44:44 + len(ver_b)] = ver_b
    raw[60:60 + len(chan_b)] = chan_b
    struct.pack_into("<I", raw, 68, caps)
    struct.pack_into("<I", raw, 72, OTA_MANIFEST_SIZE)
    struct.pack_into("<I", raw, 76, elf_size)
    raw[80:80 + len(path_b)] = path_b
    return bytes(raw)


def main() -> int:
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <input.elf> <output.agentpkg>", file=sys.stderr)
        return 1

    elf_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    elf = elf_path.read_bytes()
    if len(elf) < 64:
        print("elf too small", file=sys.stderr)
        return 1

    manifest = build_manifest(
        name="worker",
        version="2.0.0",
        channel="stable",
        caps=0x9,
        install_path="/agent/1/worker.agent",
        elf_size=len(elf),
        sig=0,
    )
    sig = package_sig(manifest, elf)
    manifest = build_manifest(
        name="worker",
        version="2.0.0",
        channel="stable",
        caps=0x9,
        install_path="/agent/1/worker.agent",
        elf_size=len(elf),
        sig=sig,
    )
    out_path.write_bytes(manifest + elf)
    print(f"[mk-agentpkg] {out_path} elf={len(elf)} sig=0x{sig:08x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
