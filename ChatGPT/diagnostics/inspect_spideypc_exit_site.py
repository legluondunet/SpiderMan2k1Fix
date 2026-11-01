#!/usr/bin/env python3
"""Offline, read-only static audit of the retail Spider-Man PC exit-call site.

Used with Wine relay evidence:
    0160:Call KERNEL32.ExitProcess(80040209) ret=0052a1c6

Run against your *own* unchanged SpideyPC.exe:
    python3 ChatGPT/diagnostics/inspect_spideypc_exit_site.py /path/to/SpideyPC.exe

This script NEVER writes to the game binary or the Wine prefix. It gates
all address interpretation on the exact SHA-256 retail target. If GNU
objdump / llvm-objdump is present, it prints the x86 instructions around
the immediate caller (not a full call stack). Otherwise raw bytes are
printed with virtual addresses. DIERR_UNPLUGGED in DirectInput shares 0x80040209 with DirectShow's
VFW_E_NOT_CONNECTED. Here the actual joystick Acquire failure path is
established by static disassembly and the prior Wine unplug trace.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import sys

SHA256 = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
SIZE = 1507328
RETURN_VA = 0x0052A1C6
EXIT_STATUS = 0x80040209


def parse_sections(binary: bytes):
    if len(binary) < 0x40 or binary[:2] != b"MZ":
        raise ValueError("Missing DOS MZ header")
    pe_off = struct.unpack_from("<I", binary, 0x3C)[0]
    if pe_off + 24 > len(binary) or binary[pe_off:pe_off + 4] != b"PE\0\0":
        raise ValueError("Missing PE signature")
    nsects = struct.unpack_from("<H", binary, pe_off + 6)[0]
    opt_size = struct.unpack_from("<H", binary, pe_off + 20)[0]
    opt = pe_off + 24
    if opt + opt_size > len(binary) or opt_size < 32:
        raise ValueError("Truncated PE optional header")
    if struct.unpack_from("<H", binary, opt)[0] != 0x10B:
        raise ValueError("Expected x86 PE32 (not PE32+)")
    base = struct.unpack_from("<I", binary, opt + 28)[0]
    table = opt + opt_size
    if table + nsects * 40 > len(binary):
        raise ValueError("Truncated PE sections")
    sections = []
    for i in range(nsects):
        off = table + i * 40
        name = binary[off:off + 8].rstrip(b"\0").decode("ascii", "replace")
        vsz, rva, rawsz, rawoff = struct.unpack_from("<IIII", binary, off + 8)
        if rawoff + rawsz > len(binary):
            raise ValueError(f"Truncated raw section {name}")
        sections.append((name, rva, vsz, rawoff, rawsz))
    return base, sections


def file_offset_to_va(raw_off: int, base: int, sections):
    for name, rva, vsz, file_off, rawsz in sections:
        if file_off <= raw_off < file_off + rawsz:
            return base + rva + raw_off - file_off, name
    return None, None


def va_to_offset(va: int, base: int, sections):
    rva = va - base
    for name, section_rva, vsz, off, rawsz in sections:
        if section_rva <= rva < section_rva + max(vsz, rawsz):
            rel = rva - section_rva
            if rel >= rawsz:
                raise ValueError(f"VA 0x{va:08X} points to a section's unbacked virtual bytes")
            return off + rel, name
    raise ValueError(f"VA 0x{va:08X} not mapped to a PE section")


def collect_evidence(path: Path, binary: bytes, *, enforce_sha=True):
    digest = hashlib.sha256(binary).hexdigest()
    if enforce_sha and (len(binary) != SIZE or digest != SHA256):
        raise ValueError(
            "Unsupported SpideyPC.exe: size or SHA-256 mismatch; "
            "refusing address assumptions (must be exact unmodified retail EXE)"
        )
    base, sections = parse_sections(binary)
    start_va = RETURN_VA - 0xC0
    end_va = RETURN_VA + 0x90
    start_off, name = va_to_offset(start_va, base, sections)
    end_off, _ = va_to_offset(end_va - 1, base, sections)
    if end_off < start_off:
        raise ValueError("Noncontiguous raw bytes around return VA")
    needle = struct.pack("<I", EXIT_STATUS)
    hits = []
    pos = 0
    while True:
        pos = binary.find(needle, pos)
        if pos == -1:
            break
        va, section = file_offset_to_va(pos, base, sections)
        hits.append((pos, va, section))
        pos += 1
    return digest, base, sections, start_va, end_va, binary[start_off:end_off + 1], hits


def render_report(path: Path, binary: bytes, *, enforce_sha=True, disassemble=True):
    digest, base, sections, lo, hi, region, hits = collect_evidence(
        path, binary, enforce_sha=enforce_sha
    )
    print("RETRO-EXE-AUDIT: target", path)
    print(f"RETRO-EXE-AUDIT: size={len(binary)} sha256={digest} base=0x{base:08X}")
    print(f"RETRO-EXE-AUDIT: observed ExitProcess(0x{EXIT_STATUS:08X}) return=0x{RETURN_VA:08X}")
    print("RETRO-EXE-AUDIT: return VA is the IMMEDIATE caller, not necessarily the original error source")
    print("RETRO-EXE-AUDIT: inspect preceding call and surrounding CRT wrapper; identify higher callers separately")
    print("RETRO-EXE-AUDIT: status 0x80040209 = DIERR_UNPLUGGED in dinput.h (also VFW_E_NOT_CONNECTED in DirectShow)")
    print("RETRO-EXE-AUDIT: original joystick Poll(0x00501E79) -> Acquire(0x00501E9F)")
    print("RETRO-EXE-AUDIT: unhandled negative Acquire HRESULT -> error formatter(0x004FC240), native CRT exit(0x00501EE7)")
    signatures = (
        (0x00501E79, bytes.fromhex("ff 51 64"), "native-joystick-Poll"),
        (0x00501E7C, bytes.fromhex("3d 1e 00 07 80"), "Poll-DIERR_INPUTLOST"),
        (0x00501E83, bytes.fromhex("3d 0c 00 07 80"), "Poll-DIERR_NOTACQUIRED"),
        (0x00501E9F, bytes.fromhex("8b 10 ff 52 1c"), "native-joystick-Acquire"),
        (0x00501EA6, bytes.fromhex("81 fe 05 00 07 80"), "Acquire-access-denied-graceful"),
        (0x00501EC6, bytes.fromhex("85 f6 74 22"), "Acquire-check-HRESULT"),
        (0x00501EDD, bytes.fromhex("85 f6 7d 0b"), "Acquire-negative-fatal"),
        (0x00501EE6, bytes.fromhex("56"), "Acquire-push-exit-status"),
        (0x00501EE7, bytes.fromhex("e8 15 82 02 00"), "Acquire-to-CRT-exit"),
        (0x0052A101, bytes.fromhex("6a 00 6a 00"), "CRT-exit-wrapper"),
        (0x0052A1C0, bytes.fromhex("ff 15 cc b0 53 00"), "CRT-import-ExitProcess"),
    )
    for va, expected, label in signatures:
        off, _section = va_to_offset(va, base, sections)
        actual = binary[off:off + len(expected)]
        if actual != expected:
            raise ValueError(f"Unexpected bytes at {label} 0x{va:08X}")
        print(f"RETRO-EXE-AUDIT: verified {label} at 0x{va:08X}: {actual.hex(' ')}")
    joystick_lo, _ = va_to_offset(0x00501E50, base, sections)
    joystick_hi, _ = va_to_offset(0x00501F12, base, sections)
    print("RETRO-EXE-AUDIT: original native joystick Poll/Acquire branch:")
    joydis = (shutil.which("objdump") or shutil.which("llvm-objdump")) if disassemble else None
    if joydis:
        proc = subprocess.run([joydis, "-d", "-M", "intel",
                               "--start-address", "0x00501e50",
                               "--stop-address", "0x00501f12", str(path)],
                              text=True, capture_output=True, check=False)
        if proc.returncode == 0:
            print(proc.stdout)
    else:
        print(binary[joystick_lo:joystick_hi].hex(" "))
    for name, rva, _, rawoff, rawsz in sections:
        print(f"RETRO-EXE-AUDIT: section {name} rva=0x{rva:08X} raw=0x{rawoff:X} size=0x{rawsz:X}")
    print(f"RETRO-EXE-AUDIT: ExitProcess immediate caller range 0x{lo:08X}..0x{hi:08X}")
    print("RETRO-EXE-AUDIT: exact return-adjacent bytes", region[0xBA:0xCA].hex(" "))
    print(f"RETRO-EXE-AUDIT: 0x{EXIT_STATUS:08X} little-endian literal file hits={len(hits)}")
    for file_off, va, section in hits[:40]:
        print(f"RETRO-EXE-AUDIT: constant raw=0x{file_off:X} VA={f'0x{va:08X}' if va is not None else 'unmapped'} section={section}")
    if len(hits) > 40:
        print(f"RETRO-EXE-AUDIT: {len(hits)-40} additional literal hits omitted")
    disassembler = (shutil.which("objdump") or shutil.which("llvm-objdump")) if disassemble else None
    if disassembler:
        print(f"RETRO-EXE-AUDIT: objdump={disassembler}")
        command = [
            disassembler, "-d", "-M", "intel",
            "--start-address", hex(lo), "--stop-address", hex(hi),
            str(path),
        ]
        proc = subprocess.run(command, text=True, capture_output=True, check=False)
        if proc.returncode == 0 and proc.stdout.strip():
            print(proc.stdout)
            return
        print(f"RETRO-EXE-AUDIT: disassembler failed (exit={proc.returncode}); falling back to raw bytes")
        if proc.stderr:
            print(proc.stderr[:800], file=sys.stderr)
    for offset in range(0, len(region), 16):
        print(f"0x{lo+offset:08X}  {region[offset:offset+16].hex(' ')}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path, help="exact supported retail SpideyPC.exe")
    args = parser.parse_args()
    try:
        binary = args.exe.read_bytes()
        render_report(args.exe, binary)
    except (OSError, ValueError) as error:
        print(f"RETRO-EXE-AUDIT: REFUSED: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
