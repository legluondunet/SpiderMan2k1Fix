#!/usr/bin/env python3
"""Offline *read-only* inspection of the exact Spider-Man 2001 PC input reader.

This is a source-guided byte scanner, NOT an x86 disassembler or a runtime
hook. It rejects any EXE whose SHA-256/size differs from the verified retail
SpideyPC.exe. Vtable-offset candidates must be disassembled independently.
No game executable is distributed with this tool.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

EXPECTED_SHA256 = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
EXPECTED_SIZE = 1507328
EXPECTED_IMAGE_BASE = 0x400000
NATIVE_READER = 0x501E6D
ORIGINAL_GUARD = bytes.fromhex("A1 2C 7A 6B 00 85 C0")
ACTION_TABLE = 0x568408
ACTION_MASKS = (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 8192, 32768)
VTABLE_OFFSETS = {
    0x1C: "Acquire", 0x20: "Unacquire",
    0x24: "GetDeviceState", 0x28: "GetDeviceData", 0x64: "Poll",
}
MAX_SIZE = 32 * 1024 * 1024
# Exact native instruction signatures already verified in
# native_joystick_unplug.c for the same supported retail binary.
# These are independently useful anchors; do not infer unlisted methods.
KNOWN_NATIVE_SIGNATURES = (
    (0x00501E79, "ff 51 64", "Poll indirect COM call"),
    (0x00501E7C, "3d 1e 00 07 80", "Poll HRESULT comparison"),
    (0x00501E83, "3d 0c 00 07 80", "Poll HRESULT comparison"),
    (0x00501E9F, "8b 10 ff 52 1c", "Acquire indirect COM call sequence"),
    (0x00501EA6, "81 fe 05 00 07 80", "Acquire HRESULT comparison"),
    (0x00501EAC, "75 18", "Original conditional branch"),
    (0x00501EBB, "33 c0 5f", "Existing nonfatal return"),
    (0x00501ECA, "68 ea 02 00 00", "Reader branch anchor"),
    (0x00501EDD, "85 f6 7d 0b", "Reader branch anchor"),
    (0x00501EE7, "e8 15 82 02 00", "Reader call anchor"),
)

# User-supplied exact same EXE independently disassembled offline with
# llvm-objdump on 2026-11-01. Each site below was checked directly against
# the authenticated file and is an instruction boundary; unlike the
# heuristic byte-scan results, these are actual static call/data-flow sites.
# No original executable bytes are redistributed.
BINARY_VERIFIED_INPUT_SITES = (
    (0x00501F0E, "ff 52 28", "original joystick GetDeviceData vtable +0x28"),
    (0x00501F13, "b1 7f", "clear button high bit before queued edges"),
    (0x00501F4F, "83 f8 30", "DIJOYSTATE button event starts at offset 0x30"),
    (0x00501F54, "8b 1d 58 7a 6b 00", "bound button count from native global"),
    (0x00501F7D, "8b 41 04", "axis/event data read for lX offset 0"),
    (0x00501F89, "8b 41 04", "axis/event data read for lY offset 4"),
    (0x00501F95, "8b 41 04", "axis/event data read for POV offset 0x20"),
    (0x0050A2AD, "e8 9e 7b ff ff", "action aggregator invokes native joystick reader"),
    (0x00505738, "e8 53 4a 00 00", "game input loop invokes action aggregator"),
    (0x0050A3A1, "e8 0a 7c ff ff", "action aggregator reads native button latch"),
)


# Independently disassembled on the user's exact retail EXE on
# 2026-11-01, with llvm-objdump --x86-asm-syntax=intel.
# There is one direct call of 0x0050A190 at 0x00505738. The caller
# passes two output DWORD pointers, but reads ONLY the first (held)
# in its subsequent 20 per-action dispatches to 0x00479540.
# This is specifically a property of the 0x00505720 caller.
# It is not proof that every other context ignores all fresh presses.
BINARY_VERIFIED_MASK_CONSUMER_SITES = (
    (0x00505720, "a1 9c 12 66 00", "frame input consumer entry"),
    (0x00505725, "83 ec 08", "reserve two DWORD output locals"),
    (0x00505729, "8d 4c 24 00", "pointer to held output local"),
    (0x00505732, "8d 44 24 04", "pointer to fresh output local"),
    (0x00505738, "e8 53 4a 00 00", "direct call of 0x0050A190 aggregator"),
    (0x00505759, "8b 54 24 08", "load held output after push ESI/EDI"),
    (0x00505785, "e8 b6 3d f7 ff", "first action-state latch update"),
    (0x005058F5, "e8 a6 3d f7 ff", "post-latch state update"),
    (0x00479540, "8b 44 24 04", "stateful 16-byte action latch updater"),
    (0x00479548, "8b 48 0c", "read frame/release counter at latch +12"),
    (0x0047954B, "8a 18", "read previous held state byte at latch +0"),
    (0x0047955D, "83 f9 0a", "new-press counter comparison against 10"),
    (0x00479560, "c6 40 01 01", "write new-press indicator to latch +1"),
    (0x0047956F, "88 50 02", "clear transient indicator latch +2"),
    (0x0047957C, "c6 00 01", "record held state +1"),
    (0x00479581, "88 10", "record released state +0"),
    (0x00479596, "8b 48 08", "read released-state counter latch +8"),
)

# These exact x86 bytes were freshly verified against the user-supplied
# 1,507,328-byte SHA-verified retail EXE on 2026-11-01. They establish
# keyboard and native joystick mask contribution as different stages,
# NOT an authenticated or safe runtime hook boundary.
BINARY_VERIFIED_SOURCE_STAGE_SITES = (
    (0x0050A19F, "c7 03 00 00 00 00", "Initialize HELD output, keep keyboard state"),
    (0x0050A1A5, "c7 45 00 00 00 00 00", "Initialize FRESH output"),
    (0x0050A1AC, "e8 cf 79 ff ff", "Keyboard reader direct call"),
    (0x0050A1B3, "0f 8c e5 00 00 00", "Keyboard error skips keyboard mapping, enters native stage"),
    (0x0050A24B, "09 10", "OR keyboard HELD into output"),
    (0x0050A283, "09 08", "OR keyboard FRESH into output"),
    (0x0050A29E, "68 94 83 56 00", "Begin game-owned DirectInput reader argument setup"),
    (0x0050A2AD, "e8 9e 7b ff ff", "Call native game joystick buffered reader"),
    (0x0050A2B5, "85 c0 0f 84 37 01 00 00", "Skip native axis/button additions if joystick reader returns zero"),
    (0x0050A371, "8b 13 0b d1 89 13", "OR native direction flags into HELD output"),
    (0x0050A3B8, "89 03", "Store merged native joystick HELD action mask"),
    (0x0050A3DF, "0b c2 89 45 00", "OR native joystick FRESH action mask"),
    (0x0050A3F4, "5f 5e 5d 5b 83 c4 0c c3", "Common epilogue preserving earlier keyboard results"),
)

# Independently checked against the same exact original retail EXE.
# Keyboard device is DISTINCT from game-owned joystick device: disabling
# all DirectInput would also disable the game keyboard, violating the
# planned generic SDL3 source-isolation contract.
BINARY_VERIFIED_KEYBOARD_READER_SITES = (
    (0x00501B9D, "a1 5c 7a 6b 00", "Original keyboard owns a DISTINCT DirectInput device at 0x006B7A5C"),
    (0x00501BC3, "ff 51 28", "Original keyboard GetDeviceData (first read)"),
    (0x00501BD9, "ff 51 1c", "Original keyboard Acquire retry"),
    (0x00501C3B, "ff 52 28", "Original keyboard GetDeviceData after Acquire"),
    (0x00501C56, "8a 98 2c 79 6b 00", "Read one of 256 keyboard latch bytes"),
    (0x00501C5E, "88 98 2c 79 6b 00", "Clear keyboard latch high/edge bit"),
    (0x00501C65, "3d 00 01 00 00", "Bound keyboard latch maintenance to 256 bytes"),
    (0x00501C87, "88 87 2c 79 6b 00", "Write pressed key event to original keyboard latch"),
    (0x00501C8F, "88 9f 2c 79 6b 00", "Write released key event to original keyboard latch"),
    (0x00501CB9, "8a 80 2c 79 6b 00", "Read latched keyboard state for semantic action"),
)


class InvalidExecutable(ValueError):
    """Binary identity, layout or native-reader signature rejected."""


@dataclass(frozen=True)
class Section:
    name: str
    rva: int
    raw_offset: int
    raw_size: int


@dataclass(frozen=True)
class PE:
    data: bytes
    base: int
    headers: int
    sections: tuple[Section, ...]

    def va_offset(self, va: int, size: int = 1) -> int:
        if va < self.base or size < 0:
            raise InvalidExecutable("invalid VA/size")
        rva = va - self.base
        if rva < self.headers:
            if rva + size <= min(self.headers, len(self.data)):
                return rva
            raise InvalidExecutable("header range not backed by file")
        matches = []
        for sec in self.sections:
            relative = rva - sec.rva
            if 0 <= relative and relative + size <= sec.raw_size:
                off = sec.raw_offset + relative
                if off + size <= len(self.data):
                    matches.append(off)
        if len(matches) != 1:
            raise InvalidExecutable("VA 0x%08X not uniquely file-backed" % va)
        return matches[0]

    def read(self, va: int, size: int) -> bytes:
        offset = self.va_offset(va, size)
        return self.data[offset:offset + size]


def parse_pe(data: bytes) -> PE:
    if len(data) < 256 or data[:2] != b"MZ":
        raise InvalidExecutable("missing MZ")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe < 0x40 or pe + 24 > len(data) or data[pe:pe+4] != b"PE\0\0":
        raise InvalidExecutable("invalid PE signature/offset")
    coff = pe + 4
    machine, count = struct.unpack_from("<HH", data, coff)
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    if machine != 0x14C or not 1 <= count <= 64 or opt_size < 0x60:
        raise InvalidExecutable("expected x86 PE32")
    if opt + opt_size + count * 40 > len(data):
        raise InvalidExecutable("truncated section table")
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise InvalidExecutable("not PE32")
    base = struct.unpack_from("<I", data, opt + 28)[0]
    headers = struct.unpack_from("<I", data, opt + 60)[0]
    if not 0 < headers <= len(data):
        raise InvalidExecutable("invalid header size")
    sections = []
    for i in range(count):
        pos = opt + opt_size + i * 40
        name, _, rva, length, file_pos = struct.unpack_from("<8sIIII", data, pos)
        if length and (file_pos >= len(data) or length > len(data) - file_pos):
            raise InvalidExecutable("section outside file")
        sections.append(Section(name.split(b"\0", 1)[0].decode("ascii", "replace"),
                                rva, file_pos, length))
    return PE(data, base, headers, tuple(sections))


def indirect_call_patterns(raw: bytes, va: int) -> list[dict]:
    """Raw FF /2 with mod=01 or 10 and no SIB; NOT verified instructions."""
    found = []
    for i in range(max(0, len(raw) - 2)):
        if raw[i] != 0xFF:
            continue
        op = raw[i + 1]
        mode, kind, rm = op >> 6, (op >> 3) & 7, op & 7
        if kind != 2 or rm == 4 or mode not in (1, 2):
            continue
        n = 3 if mode == 1 else 6
        if i + n > len(raw):
            continue
        disp = raw[i + 2] if mode == 1 else struct.unpack_from("<i", raw, i + 2)[0]
        if disp in VTABLE_OFFSETS:
            found.append({
                "va": "0x%08X" % (va + i),
                "hex": raw[i:i+n].hex(" "),
                "possible_method": VTABLE_OFFSETS[disp],
                "status": "unverified_byte_pattern_not_disassembly",
            })
    return found


def direct_relative_call_candidates(pe: PE, target: int) -> list[str]:
    """Search .text for possible x86 E8 rel32 calls to an exact target.

    These are candidate instruction bytes, not a disassembler. On the
    original EXE, their counts/addresses have also been cross-checked
    with offline llvm-objdump. Never infer a game entry hook from them.
    """
    matches = []
    for section in pe.sections:
        if section.name != ".text":
            continue
        raw = pe.data[section.raw_offset:section.raw_offset+section.raw_size]
        start = pe.base + section.rva
        for index in range(len(raw)-4):
            if raw[index] != 0xE8:
                continue
            relative = struct.unpack_from("<i", raw, index+1)[0]
            if start+index+5+relative == target:
                matches.append("0x%08X" % (start+index))
    return matches


def inspect(data: bytes, *, before: int = 96, after: int = 1024,
            expected_sha: str = EXPECTED_SHA256,
            expected_size: int = EXPECTED_SIZE) -> dict:
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != expected_size or digest != expected_sha:
        raise InvalidExecutable("unsupported EXE: exact SHA-256 and size required")
    if not (0 <= before <= 4096 and 0 <= after <= 4096):
        raise InvalidExecutable("inspection window must be bounded to 4096 bytes")
    pe = parse_pe(data)
    if pe.base != EXPECTED_IMAGE_BASE or pe.read(NATIVE_READER, len(ORIGINAL_GUARD)) != ORIGINAL_GUARD:
        raise InvalidExecutable("unsupported base or original joystick reader guard")
    known_signatures = []
    for va, hex_bytes, meaning in KNOWN_NATIVE_SIGNATURES:
        signature = bytes.fromhex(hex_bytes)
        if pe.read(va, len(signature)) != signature:
            raise InvalidExecutable("source-confirmed native reader signature differs at 0x%08X" % va)
        known_signatures.append({"va": "0x%08X" % va,
                                 "bytes": hex_bytes, "existing_source_meaning": meaning})
    binary_verified = []
    for va, hex_bytes, meaning in BINARY_VERIFIED_INPUT_SITES:
        signature = bytes.fromhex(hex_bytes)
        if pe.read(va, len(signature)) != signature:
            raise InvalidExecutable(
                "disassembled joystick dataflow site mismatch at 0x%08X" % va)
        binary_verified.append({"va": "0x%08X" % va,
                                "bytes": hex_bytes,
                                "binary_disassembly_meaning": meaning})
    consumer_signatures = []
    for va, hex_bytes, meaning in BINARY_VERIFIED_MASK_CONSUMER_SITES:
        signature = bytes.fromhex(hex_bytes)
        if pe.read(va, len(signature)) != signature:
            raise InvalidExecutable(
                "disassembled mask consumer mismatch at 0x%08X" % va)
        consumer_signatures.append({"va": "0x%08X" % va,
                                    "bytes": hex_bytes,
                                    "binary_disassembly_meaning": meaning})
    source_stage_signatures = []
    for va, hex_bytes, meaning in BINARY_VERIFIED_SOURCE_STAGE_SITES:
        if pe.read(va, len(bytes.fromhex(hex_bytes))) != bytes.fromhex(hex_bytes):
            raise InvalidExecutable(
                "keyboard/joystick stage signature mismatch at 0x%08X" % va)
        source_stage_signatures.append({
            "va": "0x%08X" % va,
            "bytes": hex_bytes,
            "binary_disassembly_meaning": meaning,
        })
    keyboard_signatures = []
    for va, hex_bytes, meaning in BINARY_VERIFIED_KEYBOARD_READER_SITES:
        if pe.read(va, len(bytes.fromhex(hex_bytes))) != bytes.fromhex(hex_bytes):
            raise InvalidExecutable(
                "keyboard native DirectInput reader signature mismatch at 0x%08X" % va)
        keyboard_signatures.append({
            "va": "0x%08X" % va,
            "bytes": hex_bytes,
            "binary_disassembly_meaning": meaning,
        })
    native = pe.read(ACTION_TABLE, 14 * 8)
    pairs = [struct.unpack_from("<II", native, i * 8) for i in range(14)]
    if tuple(mask for mask, _ in pairs) != ACTION_MASKS:
        raise InvalidExecutable("native action-table masks differ")
    if any(button > 127 and button != 0x4000 for _, button in pairs):
        raise InvalidExecutable("invalid native action-table index")
    start = NATIVE_READER - before
    raw = pe.read(start, before + after)
    return {
        "provenance": "exact retail SHA256, preexisting source-known addresses, raw bytes only",
        "sha256": digest, "bytes": len(data),
        "image_base": "0x%08X" % pe.base,
        "sections": [s.__dict__ for s in pe.sections],
        "existing_native_pointers": {
            "joystick": "0x006B7A2C", "factory": "0x006B7A30",
            "data_format": "0x0053D0F4",
        },
        "reader_null_guard": {
            "va": "0x%08X" % NATIVE_READER,
            "bytes": ORIGINAL_GUARD.hex(" "),
        },
        "verified_previous_source_signatures": known_signatures,
        "verified_binary_input_sites": binary_verified,
        "verified_binary_mask_consumer_sites": consumer_signatures,
        "verified_keyboard_native_source_stage_sites": source_stage_signatures,
        "verified_original_keyboard_reader_sites": keyboard_signatures,
        "original_keyboard_directinput": {
            "keyboard_device_pointer": "0x006B7A5C",
            "keyboard_state_bytes": "0x006B792C..0x006B7A2B",
            "keyboard_latch_entries": 256,
            "keyboard_buffered_event_capacity": 16,
            "keyboard_buffered_event_record_bytes": 20,
            "first_buffered_get_device_data_call": "0x00501BC3",
            "reacquire_then_buffered_read": "0x00501BD9 / 0x00501C3B",
            "keyboard_state_getter": "0x00501CB0",
            "press_marker": "0xFF",
            "release_marker": "0x80",
            "separate_from_native_joystick_pointer": "0x006B7A2C",
            "critical": "Do NOT disable DirectInput globally; the keyboard relies on a distinct original DI device",
        },
        "source_separation": {
            "original_formula": "held = keyboard | native DirectInput joystick",
            "keyboard_region": "0x0050A1AC..0x0050A29D",
            "native_joystick_region": "0x0050A29E..0x0050A3F3",
            "keyboard_held_or": "0x0050A24B",
            "native_direction_or": "0x0050A371",
            "native_button_held_or": "0x0050A3B8",
            "skip_native_on_zero": "0x0050A2B5 -> 0x0050A3F4",
            "generic_bridge_source_isolation": "NOT IMPLEMENTED",
            "native_side_effects_when_skipped": "UNKNOWN",
            "game_hook_authorized": False,
        },
        "original_direct_call_xrefs": {
            # Binary-disassembly comparison on the exact retail executable
            # showed these same addresses as the raw E8 candidates.
            "aggregator_0x0050A190": direct_relative_call_candidates(pe,0x0050A190),
            "consumer_0x00505720": direct_relative_call_candidates(pe,0x00505720),
            "state_updater_0x00479540": direct_relative_call_candidates(pe,0x00479540),
            "post_update_0x004796A0": direct_relative_call_candidates(pe,0x004796A0),
            "note": "Raw E8 candidates, cross-checked with offline disassembly on this SHA only; not an authorized hook",
        },
        "original_engine_mask_consumer": {
            "aggregator": "0x0050A190",
            "only_direct_xref_in_static_disassembly": "0x00505738",
            "consumer": "0x00505720",
            "held_output_local": "ESP+0 at aggregator call",
            "fresh_output_local": "ESP+4 at aggregator call",
            "first_consumed_output": "held only in this consumer",
            "action_latch_updater": "0x00479540",
            "number_of_direct_latch_calls": 20,
            "latch_layout": "16 bytes: held byte +0, transient bytes +1/+2, counters +4/+8/+12",
            "safety_note": "Static function-specific evidence, not an authorized input hook or proof of analog equivalence",
        },
        "joystick_buffered_dataflow": {
            "reader": "0x00501E50",
            "event_call": "0x00501F0E",
            "event_record_size": 20,
            "event_queue_limit": 16,
            "button_event_offsets": "0x30 <= dwOfs < 0x30+native-button-count",
            "button_latch": "0x006B7A34..0x006B7A53",
            "axis_lx_event_offset": 0,
            "axis_ly_event_offset": 4,
            "pov_event_offset": 32,
            "axis_lx_destination": "0x00AC0A10",
            "axis_ly_destination": "0x00AC0A14",
            "pov_destination": "0x00568394",
            "aggregator": "0x0050A190",
            "frame_input_caller": "0x00505720",
            "warn": "Static binary evidence only: not a validated live SDL3 injection point.",
        },
        "source_known_com_calls": [
            {"va": "0x00501E79", "method": "Poll",
             "proof": "existing native_joystick_unplug.c exact three-byte signature"},
            {"va": "0x00501EA1", "method": "Acquire",
             "proof": "existing native_joystick_unplug.c five-byte sequence at 0x00501E9F"},
        ],
        "axis_setup_samples": {
            "lX_setup_at_0x005019E0": pe.read(0x5019E0, 16).hex(" "),
            "lY_setup_at_0x00501A42": pe.read(0x501A42, 16).hex(" "),
        },
        "native_action_table": {
            "va": "0x%08X" % ACTION_TABLE,
            "all_button_indices": [button for _, button in pairs],
            "gameplay_indices_4_to_10": [button for _, button in pairs[4:11]],
        },
        "reader_window": {
            "start": "0x%08X" % start, "end_exclusive": "0x%08X" % (start + len(raw)),
            "indirect_call_byte_candidates": indirect_call_patterns(raw, start),
        },
        "limit": "Candidates are byte matches, not decoded x86/COM call sites; no live-game or DS3 compatibility claim.",
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", type=Path, help="Original SHA-verified 2001 SpideyPC.exe")
    ap.add_argument("--before", type=int, default=96)
    ap.add_argument("--after", type=int, default=1024)
    args = ap.parse_args(argv)
    try:
        if args.exe.stat().st_size > MAX_SIZE:
            raise InvalidExecutable("file size exceeds inspection limit")
        result = inspect(args.exe.read_bytes(), before=args.before, after=args.after)
    except (OSError, InvalidExecutable) as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
