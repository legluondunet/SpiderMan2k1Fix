#!/usr/bin/env python3
"""Offline, read-only retail GAME OVER ownership evidence.

Checks the EXACT original Spider-Man PC 2001 executable. No game binary
is redistributed. No executable patch, game process, or keyboard input.
This tool authenticates static instruction/data evidence only; it cannot
establish runtime liveness or safety of a default-on controller bridge.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

RETAIL_SIZE = 1507328
RETAIL_SHA256 = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
IMAGE_BASE = 0x00400000

# Independently decoded from the exact SHA-verified EXE using llvm-objdump
# --x86-asm-syntax=intel. Each address is an instruction boundary.
SIGNATURES = {
    "native_3d_frame_draw_calls_0x440af0":
        (0x0045599C, "e8 4f b1 fe ff"),
    "gameover_title_actor_ptr":
        (0x00440B9C, "a1 38 90 6a 00"),
    "gameover_title_actor_alive_guard":
        (0x00440BA1, "85 c0 74 75"),
    "gameover_title_signed_hp_nonpositive":
        (0x00440BA5, "66 83 b8 e2 00 00 00 00"),
    "gameover_title_positive_hp_skip":
        (0x00440BAD, "7f 6b"),
    "gameover_title_phase_must_equal_one":
        (0x00440BAF, "83 3d b0 b1 5f 00 01"),
    "gameover_title_phase_skip":
        (0x00440BB6, "75 62"),
    "gameover_native_source_text_reference":
        (0x00440BDF, "8b 15 48 b7 54 00"),
    "gameover_3d_death_elapsed_ticks":
        (0x004B28A5, "83 3d 98 7f 6a 00 78"),
    "gameover_3d_death_clears_source_title_phase":
        (0x004B28AE, "a3 b0 b1 5f 00"),
    "gameover_3d_death_requests_controller_mode_two":
        (0x004B28B3, "c7 05 a4 cf 60 00 02 00 00 00"),
    "other_mode_two_writer_first":
        (0x004459BD, "c7 05 a4 cf 60 00 02 00 00 00"),
    "other_mode_two_writer_second":
        (0x00455BE9, "c7 05 a4 cf 60 00 02 00 00 00"),
    "other_mode_two_writer_third":
        (0x004BE027, "c7 05 a4 cf 60 00 02 00 00 00"),
    "controller_mode_two_jump_target":
        (0x00455F5D, "53"),
    "mode_two_gameover_screen_draws_source_text":
        (0x0047D93A, "a1 48 b7 54 00"),
    "mode_two_gameover_screen_polls_native_input":
        (0x0047D9B8, "e8 c3 e7 08 00"),
}
SOURCE_PTR_ENTRY = 0x0054B748
SOURCE_TEXT_VA = 0x0054D008


class InvalidExecutable(ValueError):
    """Refuse altered, unrecognized or structurally invalid retail binaries."""


def map_pe_sections(data: bytes):
    if len(data) != RETAIL_SIZE or hashlib.sha256(data).hexdigest() != RETAIL_SHA256:
        raise InvalidExecutable("unsupported original EXE size or SHA-256")
    if data[:2] != b"MZ":
        raise InvalidExecutable("missing DOS header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise InvalidExecutable("missing PE signature")
    count, optional_size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x10B:
        raise InvalidExecutable("not a PE32 image")
    if struct.unpack_from("<I", data, optional + 28)[0] != IMAGE_BASE:
        raise InvalidExecutable("unexpected image base")
    sections = []
    for i in range(count):
        entry = pe + 24 + optional_size + i * 40
        name, virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
            "<8sIIII", data, entry)
        sections.append((name.rstrip(b"\0").decode("ascii", "replace"),
                         rva, virtual_size, raw_offset, raw_size))
    return sections


def read_va(data: bytes, sections, va: int, n: int) -> bytes:
    rva = va - IMAGE_BASE
    for name, start, virtual_size, raw_offset, raw_size in sections:
        if start <= rva and rva + n <= start + min(virtual_size, raw_size):
            pos = raw_offset + rva - start
            if pos + n > len(data):
                break
            return data[pos:pos + n]
    raise InvalidExecutable("VA 0x%08X has no safe mapped raw bytes" % va)


def inspect(data: bytes) -> dict:
    sections = map_pe_sections(data)
    for name, (address, hex_data) in SIGNATURES.items():
        expected = bytes.fromhex(hex_data)
        if read_va(data, sections, address, len(expected)) != expected:
            raise InvalidExecutable("source instruction mismatch: " + name)
    pointer = struct.unpack("<I", read_va(data, sections, SOURCE_PTR_ENTRY, 4))[0]
    if pointer != SOURCE_TEXT_VA:
        raise InvalidExecutable("the source GAME OVER text pointer changed")
    if read_va(data, sections, pointer, 10) != b"game over\0":
        raise InvalidExecutable("the original GAME OVER string changed")
    return {
        "verified_original_sha256": RETAIL_SHA256,
        "verified_site_count": len(SIGNATURES),
        "source_text_pointer": "0x%08X" % SOURCE_PTR_ENTRY,
        "source_text": "game over",
        "render_3d_gate": {
            "caller": "0x0045599C -> 0x00440AF0",
            "title_draw_instruction": "0x00440BDF",
            "actor": "[0x006A9038]",
            "signed_word_at_actor_offset": "+0xE2 <= 0",
            "source_phase": "[0x005FB1B0] == 1",
        },
        "one_request_two_writer_after_death": {
            "title_phase_clear": "0x004B28AE",
            "request_two_write": "0x004B28B3",
            "elapsed_ticks_check": "[0x006A7F98] >= 0x78",
        },
        "other_literal_request_two_writer_sites": [
            "0x004459BD", "0x00455BE9", "0x004BE027"
        ],
        "controller_mode_two_screen": {
            "screen_entry": "0x0047D830",
            "game_over_source_text_draw": "0x0047D93A",
            "game_native_input_poll": "0x0047D9B8 -> 0x0050C180",
        },
        "runtime_owner_claim": "NOT ESTABLISHED: inspect live frame sequences",
        "safe_default_controller_injection": False,
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path, help="User-owned original PE32 SpideyPC.exe")
    args = parser.parse_args(argv)
    try:
        if args.exe.stat().st_size != RETAIL_SIZE:
            raise InvalidExecutable("unsupported original executable size")
        result = inspect(args.exe.read_bytes())
    except (OSError, InvalidExecutable) as error:
        print("ERROR: %s" % error, file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
