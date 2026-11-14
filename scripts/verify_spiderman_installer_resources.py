#!/usr/bin/env python3
"""Fail closed if the standalone PE32 installer lacks any expected RCDATA payload.

No third-party PE library or original game files are required. Compare the
actual embedded bytes, not just the presence of a .rsrc section, against the
seven redistributable files staged by the build workflow.
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

PAYLOADS = {
    1101: "winmm.dll",
    1102: "SpiderMan2k1Fix/SpiderMan2k1Fix.dll",
    1103: "SpiderMan2k1Fix/SDL3.dll",
    1104: "SpiderMan2k1Fix/SDL3-LICENSE.txt",
    1105: "SpiderMan2k1Fix/linux/spiderman-focus-helper",
    1106: "SpiderMan2k1Fix/README.md",
    1107: "SpiderMan2k1Fix/CHANGELOG.md",
}


def verify(executable: Path, package: Path) -> None:
    binary = executable.read_bytes()

    def unpack(fmt: str, offset: int):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(binary):
            raise ValueError("truncated PE header/resource directory")
        return struct.unpack_from(fmt, binary, offset)

    if binary[:2] != b"MZ":
        raise ValueError("not an MZ executable")
    pe = unpack("<I", 0x3C)[0]
    if binary[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE executable")
    machine, nsections, _, _, _, optional_size, _ = unpack("<HHIIIHH", pe + 4)
    optional = pe + 24
    if machine != 0x14C or unpack("<H", optional)[0] != 0x10B:
        raise ValueError("installer must be PE32 x86")
    if optional_size < 96 + 3 * 8:
        raise ValueError("missing resource data directory")
    resource_rva, resource_size = unpack("<II", optional + 96 + 2 * 8)
    if not resource_rva or resource_size < 16:
        raise ValueError("missing PE resource directory")
    sections = []
    for i in range(nsections):
        offset = optional + optional_size + 40 * i
        _, virtual_size, virtual_address, raw_size, raw_offset = unpack(
            "<8sIIII", offset
        )
        if raw_offset + raw_size > len(binary):
            raise ValueError("truncated PE section")
        sections.append((virtual_address, raw_size, raw_offset, virtual_size))

    def mapped(rva: int, size: int) -> int:
        for start, raw_size, file_offset, _ in sections:
            if start <= rva and size <= raw_size and rva - start <= raw_size - size:
                return file_offset + (rva - start)
        raise ValueError("resource points outside a file-backed PE section")

    base = mapped(resource_rva, resource_size)

    def relative(offset: int, length: int) -> int:
        if offset < 0 or length > resource_size or offset > resource_size - length:
            raise ValueError("resource entry extends beyond resource directory")
        return base + offset

    def entries(directory_offset: int):
        off = relative(directory_offset, 16)
        named, numeric = unpack("<HH", off + 12)
        if named or numeric > 1024:
            raise ValueError("unexpected named/oversized PE resource directory")
        result = []
        for i in range(numeric):
            name, child = unpack("<II", relative(directory_offset + 16 + 8 * i, 8))
            if name & 0x80000000:
                raise ValueError("resource must have a numeric ID")
            result.append((name, child))
        return result

    types = [(name, child) for name, child in entries(0) if name == 10]
    if len(types) != 1 or not (types[0][1] & 0x80000000):
        raise ValueError("missing RCDATA resource type")
    found = {}
    for resource_id, child in entries(types[0][1] & 0x7FFFFFFF):
        if resource_id not in PAYLOADS or resource_id in found:
            raise ValueError(f"unexpected or duplicate RCDATA id {resource_id}")
        if not child & 0x80000000:
            raise ValueError("missing RCDATA language directory")
        languages = entries(child & 0x7FFFFFFF)
        if len(languages) != 1 or languages[0][1] & 0x80000000:
            raise ValueError("RCDATA must contain one language and one data entry")
        data_offset = relative(languages[0][1], 16)
        data_rva, size, _, _ = unpack("<IIII", data_offset)
        embedded = binary[mapped(data_rva, size):][:size]
        external = (package / PAYLOADS[resource_id]).read_bytes()
        if not external or len(embedded) != len(external) or embedded != external:
            raise ValueError(f"RCDATA {resource_id} mismatches staged {PAYLOADS[resource_id]}")
        found[resource_id] = hashlib.sha256(embedded).hexdigest()
    if found.keys() != PAYLOADS.keys():
        raise ValueError(f"missing RCDATA IDs: {sorted(PAYLOADS.keys() - found.keys())}")
    for resource_id, digest in sorted(found.items()):
        print(f"RCDATA {resource_id} SHA-256 {digest} {PAYLOADS[resource_id]}")
    print(f"PASS: all {len(PAYLOADS)} embedded resources match staged files byte for byte")


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: verify_spiderman_installer_resources.py EXE PACKAGE_DIR", file=sys.stderr)
        return 2
    try:
        verify(Path(sys.argv[1]), Path(sys.argv[2]))
    except (ValueError, OSError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
