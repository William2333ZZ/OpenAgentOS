#!/usr/bin/env python3
"""Write an AOS1-format AgentOS persist image for migration tests."""

import struct
import sys

MAGIC = 0x31495641
VERSION_AOS1 = 1
SECTOR = 512
PATH_MAX = 64


def write_sector(f, off: int, data: bytes) -> None:
    if len(data) > SECTOR:
        raise ValueError("sector overflow")
    f.seek(off * SECTOR)
    f.write(data + b"\x00" * (SECTOR - len(data)))


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <disk.img> <marker-body>", file=sys.stderr)
        sys.exit(1)

    path = sys.argv[1]
    body = sys.argv[2].encode("utf-8")
    file_path = b"/agent/1/persist2-marker"
    if len(file_path) >= PATH_MAX:
        raise SystemExit("path too long")
    if len(body) >= 512:
        raise SystemExit("body too long")

    meta = file_path + b"\x00" * (PATH_MAX - len(file_path))
    meta += struct.pack("<I", len(body))
    meta += b"\x00" * (SECTOR - len(meta))

    data_sector = 2
    superblock = struct.pack("<IIII", MAGIC, VERSION_AOS1, 1, 0)

    with open(path, "wb") as f:
        f.truncate(SECTOR * 256)
        write_sector(f, 0, superblock)
        write_sector(f, 1, meta)
        write_sector(f, data_sector, body)

    print(f"[seed-aos1] wrote {path!r} marker={body!r} (AOS1, 1 file)")


if __name__ == "__main__":
    main()
