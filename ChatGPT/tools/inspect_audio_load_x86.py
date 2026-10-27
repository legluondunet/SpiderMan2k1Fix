#!/usr/bin/env python3
"""Read-only native x86 audio-loading reconnaissance for exact Spider-Man PC 2001.

No Wine session, game launch, DLL injection, binary patch, or extra Python module.
Run: python3 ChatGPT/tools/inspect_audio_load_x86.py /path/to/SpideyPC.exe

Emits an annotated disassembly and bounded cross-reference/constant census.
Function identifications beyond the previously verified sites are CANDIDATES.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import subprocess
from pathlib import Path

EXPECTED_SIZE = 1_507_328
EXPECTED_SHA256 = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
IMAGE_BASE_EXPECTED = 0x00400000
KNOWN = {
    "voice initialization (already signature-guarded)": (0x005039F0, bytes.fromhex("83 EC 24 56 57 B9 80 00 00 00")),
    "DXSOUND_Open / duplicate path (already signature-guarded)": (0x00504110, bytes.fromhex("8B 0D 20 79 6B 00 53 56 57")),
    "native DXSOUND_Play": (0x00504230, None),
    "native DXSOUND_Stop": (0x005042A0, bytes.fromhex("8B 44 24 04 56 8D 04 40")),
    "original native Play callsite": (0x00472870, bytes.fromhex("E8 BB 19 09 00")),
}
WINDOW_START = 0x005039D0
WINDOW_END = 0x00504340
PATTERNS = {
    "0x800 (2048 bytes)": bytes.fromhex("00 08 00 00"),
    "0x7FF (2047 mask)": bytes.fromhex("FF 07 00 00"),
    "0xFFFFF800 (negative 2048)": bytes.fromhex("00 F8 FF FF"),
    "0x400 (1024 bytes)": bytes.fromhex("00 04 00 00"),
    "0x7FC (2044 bytes)": bytes.fromhex("FC 07 00 00"),
}


def u16(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


class PE32:
    def __init__(self, blob: bytes):
        if len(blob) < 0x100 or blob[:2] != b"MZ":
            raise ValueError("not an MZ executable")
        pe = u32(blob, 0x3C)
        if pe + 0x78 >= len(blob) or blob[pe:pe + 4] != b"PE\0\0":
            raise ValueError("missing PE signature")
        coff = pe + 4
        if u16(blob, coff) != 0x14C:
            raise ValueError("only i386 is supported")
        count = u16(blob, coff + 2)
        opt_size = u16(blob, coff + 16)
        opt = coff + 20
        if u16(blob, opt) != 0x10B or opt + opt_size > len(blob):
            raise ValueError("only PE32 is supported")
        self.image_base = u32(blob, opt + 28)
        self.sections = []
        first = opt + opt_size
        if count > 48 or first + count * 40 > len(blob):
            raise ValueError("invalid section table")
        for i in range(count):
            p = first + 40 * i
            name = blob[p:p + 8].split(b"\0", 1)[0].decode("ascii", "replace")
            size_virtual, rva, size_raw, raw = struct.unpack_from("<IIII", blob, p + 8)
            if raw + size_raw > len(blob):
                raise ValueError("section exceeds file")
            self.sections.append((name, rva, size_virtual, size_raw, raw))
        self.blob = blob

    def file_offset(self, va: int, length: int = 1) -> int:
        if length < 0 or va < self.image_base:
            raise ValueError(f"invalid VA 0x{va:08X}")
        rva = va - self.image_base
        for _, section_rva, _virtual, raw_size, raw in self.sections:
            if section_rva <= rva and rva + length <= section_rva + raw_size:
                return raw + rva - section_rva
        raise ValueError(f"unmapped raw VA 0x{va:08X} (+{length})")

    def read(self, va: int, length: int) -> bytes:
        off = self.file_offset(va, length)
        return self.blob[off:off + length]

    def text(self) -> tuple[int, bytes]:
        matches = [s for s in self.sections if s[0] == ".text"]
        if len(matches) != 1:
            raise ValueError("expected one .text section")
        _, rva, _virt, size, raw = matches[0]
        return self.image_base + rva, self.blob[raw:raw + size]


def candidate_rel32_calls(pe: PE32, lower: int, upper: int, limit: int = 150) -> list[tuple[int, int]]:
    """Raw E8 signature candidates, NOT validated instruction boundaries."""
    base, body = pe.text()
    found = []
    for pos in range(max(0, len(body) - 4)):
        if body[pos] != 0xE8:
            continue
        displacement = struct.unpack_from("<i", body, pos + 1)[0]
        target = (base + pos + 5 + displacement) & 0xFFFFFFFF
        if lower <= target < upper:
            found.append((base + pos, target))
            if len(found) == limit:
                break
    return found


def probe_constants(pe: PE32, lower: int, upper: int) -> list[str]:
    body = pe.read(lower, upper - lower)
    rows: list[str] = []
    for label, pattern in PATTERNS.items():
        indices = []
        start = 0
        while True:
            hit = body.find(pattern, start)
            if hit < 0:
                break
            indices.append(lower + hit)
            start = hit + 1
        rows.append(f"{label}: " + (", ".join(f"0x{x:08X}" for x in indices[:30]) or "none")
                    + (" [TRUNCATED]" if len(indices) > 30 else ""))
    return rows


def disassemble(exe: Path, start: int, stop: int) -> str:
    tool = shutil.which("objdump") or shutil.which("llvm-objdump")
    if not tool:
        return "Disassembler unavailable: install binutils (objdump) or LLVM.\n"
    cmd = [tool, "-d", "-M", "intel", f"--start-address=0x{start:X}",
           f"--stop-address=0x{stop:X}", str(exe)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=40, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return f"Disassembler failed: {exc}\n"
    if result.returncode:
        return f"Disassembler exit {result.returncode}:\n{result.stderr[:1500]}\n"
    return result.stdout


def inspect(blob: bytes, exe: Path, out: Path) -> str:
    if len(blob) != EXPECTED_SIZE:
        raise ValueError(f"unsupported SpideyPC.exe length {len(blob)} (expected {EXPECTED_SIZE})")
    sha = hashlib.sha256(blob).hexdigest()
    if sha != EXPECTED_SHA256:
        raise ValueError(f"unsupported SpideyPC.exe SHA-256 {sha}")
    pe = PE32(blob)
    if pe.image_base != IMAGE_BASE_EXPECTED:
        raise ValueError(f"unsupported image base 0x{pe.image_base:08X}")
    rows = [
        "SPIDER-MAN 2001 PC: NATIVE AUDIO LOADER ROOT-CAUSE RECONNAISSANCE",
        "STATIC / READ-ONLY; does not launch, inject, or patch the game",
        f"Input: {exe}",
        f"SHA-256: {sha} (VERIFIED)",
        f"ImageBase: 0x{pe.image_base:08X}",
        "",
        "VERIFIED ORIGINAL SIGNATURES AND KNOWN ADDRESSES",
    ]
    for name, (addr, signature) in KNOWN.items():
        raw = pe.read(addr, len(signature) if signature else 16)
        if signature and raw != signature:
            raise ValueError(f"native signature mismatch: {name}, VA 0x{addr:08X}")
        rows.append(f"{name} @ 0x{addr:08X}: {raw.hex(' ')}" +
                    (" [exact bytes MATCH]" if signature else " [address known; no code signature asserted]"))
    rows += ["", "KNOWN / HYPOTHESIZED STAGES",
             "DXSOUND_Open @ 0x00504110 duplicates a reference buffer (known from prior game probes).",
             "Source loader DXSOUND_CreateDSBuffer, loadWAV, ParseWavHeader: ADDRESSES NOT YET VERIFIED.",
             "The 2048-byte file-sector boundary is a measured PCM symptom, not yet an identified instruction.",
             "Any constant-byte match or raw E8 target below is a CANDIDATE only, not a confirmed instruction.",
             "", "RAW IMMEDIATE PATTERNS IN NATIVE AUDIO NEIGHBORHOOD",
             *probe_constants(pe, WINDOW_START, WINDOW_END), "",
             "POTENTIAL x86 CALL E8 REFERENCES INTO THIS AUDIO NEIGHBORHOOD"]
    candidates = candidate_rel32_calls(pe, WINDOW_START, WINDOW_END)
    rows += [f"caller=0x{p:08X} target=0x{t:08X}" for p, t in candidates]
    if not candidates:
        rows.append("(no raw E8 candidates found)")
    rows += ["", "NATIVE CODE NEIGHBORHOOD / objdump disassembly",
             f"VA 0x{WINDOW_START:08X}-0x{WINDOW_END:08X}",
             "Review function boundaries and COM indirect call instructions manually.",
             "Potential native DS interface vtable offsets on x86:",
             "  IDirectSound::CreateSoundBuffer +0x0C; DuplicateSoundBuffer +0x14",
             "  IDirectSoundBuffer::Lock +0x2C; Unlock +0x4C",
             "Offset appearance alone does NOT prove a DirectSound COM invocation.",
             "",
             disassemble(exe, WINDOW_START, WINDOW_END),
             "",
             "FOLLOW-UP NATIVE RESOURCE READER CALLEES: disassembly",
             "VA 0x00504C80-0x00505300; contains calls to 0x504CB0, 0x505170, 0x5051D0.",
             "A floor(size/2048) quotient is passed to 0x5051D0 from the native loader;",
             "this is a high-confidence incomplete-last-sector suspect, not yet a proven API contract.",
             disassemble(exe, 0x00504C80, 0x00505300),
             "",
             "ALTERNATIVE RESOURCE READER CALL CANDIDATES: disassembly",
             "VA 0x00430800-0x00430DC0; contains xrefs 0x430A21, 0x430B23, 0x430B52, 0x430B9E.",
             "This may be a distinct resource read path; WAV usage not yet proven.",
             disassemble(exe, 0x00430800, 0x00430DC0)]
    report = "\n".join(rows) + "\n"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(report, encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path, help="exact original SpideyPC.exe (unchanged)")
    parser.add_argument("--output", "-o", type=Path,
                        default=Path("spiderman-audio-root-disassembly.txt"))
    args = parser.parse_args()
    try:
        report = inspect(args.exe.read_bytes(), args.exe, args.output)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Verified original; saved {args.output} ({len(report)} characters).")
    print("Attach the report to the PR investigation; game and EXE remain untouched.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
