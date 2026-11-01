#!/usr/bin/env python3
"""Offline, exact-hash audit of original Spider-Man 2001 input call sites.

No runtime patch, binary redistribution, controller hooks, or synthesized
input. Raw E8 rel32 candidates were independently compared with the
maintainer's original PE32 llvm-objdump disassembly; the per-callsite
labels in this report are only *address groups*, not known game modes.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

from inspect_spidey_native_reader import (
    InvalidExecutable,
    direct_relative_call_candidates,
    inspect,
    parse_pe,
)

# Counts independently established by llvm-objdump -d on the exact
# SHA-matched retail PE32. These are not assertions about execution counts.
EXPECTED_COUNTS = {
    0x0050A190: 1,   # input aggregator, one direct call from input service
    0x00505720: 56,  # shared service, many unrelated consumers
    0x00479540: 20,  # action-state latch updater, all within shared service
    0x004796A0: 1,   # final state maintenance
}
KNOWN_CRITICAL_CALLS = {
    0x0050A190: (0x00505738,),
    0x004796A0: (0x005058F5,),
}
# These 20 xrefs were independently recorded from original disassembly.
EXPECTED_STATE_UPDATE_CALLS = (
    0x00505785, 0x00505798, 0x005057AB, 0x005057BE,
    0x005057D4, 0x005057E1, 0x005057F7, 0x0050580D,
    0x00505823, 0x00505836, 0x00505849, 0x0050585C,
    0x00505869, 0x00505876, 0x00505886, 0x00505891,
    0x005058AA, 0x005058BF, 0x005058D1, 0x005058E3,
)
# 56 verified direct-call instruction starts from the user's genuine EXE.
EXPECTED_SHARED_SERVICE_CALLS = (
    0x00427989, 0x004279C8, 0x00427A38, 0x00427A75,
    0x00427B25, 0x00427B64, 0x00427BE8, 0x00427C27,
    0x00427E60, 0x00427E9F, 0x00440F4F, 0x00443F3B,
    0x00455477, 0x00470835, 0x004793B9, 0x0047C0E9,
    0x0047D849, 0x0047D985, 0x0048DFE5, 0x0048EC85,
    0x004935BB, 0x00493DD6, 0x00496B56, 0x004973E4,
    0x00497A88, 0x004985D9, 0x0049896A, 0x00498D5E,
    0x004993B8, 0x004998A2, 0x00499CF2, 0x0049A1B2,
    0x0049A757, 0x0049AFC8, 0x0049BC8E, 0x0049BF64,
    0x0049C86B, 0x0049CB1E, 0x0049CFB1, 0x0049D485,
    0x0049D8BF, 0x0049DA34, 0x0049E234, 0x0049E950,
    0x0049ED9D, 0x0049F5D3, 0x0049FFFA, 0x004A0CDC,
    0x004A149C, 0x004A17DC, 0x004A1888, 0x004A1980,
    0x004A1A21, 0x00509883, 0x0050D31E, 0x0050DC72,
)
RETAIL_SHA = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
RETAIL_SIZE = 1507328
# These executable bytes establish a script-VM -> dispatch-table call
# chain and a native direction-reset-on-consumption path; they DO NOT
# establish which rendering context is active.
VERIFIED_SCRIPT_INPUT_CHAIN = (
    (0x004E0461, "e8 aa 74 f4 ff", "script VM calls handler 0x00427910"),
    (0x00427966, "ff 24 8d 98 7d 42 00", "handler jumps via case table"),
    (0x0050C2AE, "e8 fd e6 ff ff", "input condition consumes direction and resets gate"),
    (0x0050A9B0, "c6 05 18 0a ac 00 01", "native gate reset writes 0x00AC0A18"),
    (0x0050A2BD, "a0 18 0a ac 00", "native aggregator reads the gate"),
    (0x00427CD6, "e8 65 01 00 00", "script handler invokes separate input wait helper"),
    (0x00427D22, "e8 19 01 00 00", "script handler invokes separate input wait helper"),
    (0x00427E40, "8b 44 24 04", "entry of separate input wait helper"),
)
SCRIPT_DISPATCH_INLINE_CALLS = EXPECTED_SHARED_SERVICE_CALLS[:8]
SCRIPT_DISPATCH_HELPER_CALLS = EXPECTED_SHARED_SERVICE_CALLS[8:10]
SCRIPT_DISPATCH_HELPER_XREFS = (0x00427CD6, 0x00427D22)


def _parse_address(va: str) -> int:
    return int(va, 16)


def audit(data: bytes) -> dict:
    """Fail closed until the existing exact-sha input-reader inspection passes."""
    verified = inspect(data)
    pe = parse_pe(data)
    found: dict[int, tuple[int, ...]] = {}
    for target, expected in EXPECTED_COUNTS.items():
        addresses = tuple(_parse_address(x)
                          for x in direct_relative_call_candidates(pe, target))
        if len(addresses) != expected or len(set(addresses)) != len(addresses):
            raise InvalidExecutable("input xref count mismatch at 0x%08X" % target)
        found[target] = addresses
    for target, expected in KNOWN_CRITICAL_CALLS.items():
        if found[target] != expected:
            raise InvalidExecutable("critical direct call mismatch at 0x%08X" % target)
    if found[0x00505720] != EXPECTED_SHARED_SERVICE_CALLS:
        raise InvalidExecutable("56 verified shared service call addresses differ")
    if found[0x00479540] != EXPECTED_STATE_UPDATE_CALLS:
        raise InvalidExecutable("20 verified action state update calls differ")
    script_proofs = []
    for address, hex_bytes, meaning in VERIFIED_SCRIPT_INPUT_CHAIN:
        if pe.read(address, len(bytes.fromhex(hex_bytes))) != bytes.fromhex(hex_bytes):
            raise InvalidExecutable(
                "script-event input gate signature mismatch at 0x%08X" % address)
        script_proofs.append({
            "va": "0x%08X" % address, "bytes": hex_bytes,
            "static_role": meaning
        })
    script_inline=tuple(address for address in found[0x00505720]
                        if 0x0042796D <= address < 0x00427D92)
    script_helper=tuple(address for address in found[0x00505720]
                        if 0x00427E40 <= address < 0x00427ECF)
    helper_xrefs=tuple(_parse_address(x) for x in
                       direct_relative_call_candidates(pe,0x00427E40))
    if script_inline != SCRIPT_DISPATCH_INLINE_CALLS:
        raise InvalidExecutable("script-dispatch inline callsite set differs")
    if script_helper != SCRIPT_DISPATCH_HELPER_CALLS or \
       helper_xrefs != SCRIPT_DISPATCH_HELPER_XREFS:
        raise InvalidExecutable("script input helper chain differs")
    # Entire xref inventory is from the file's .text bytes only.
    # Addresses do not reveal which call executes in a movie/menu/3D:
    # only dynamic context tracing can establish that.
    external = found[0x00505720]
    groups: dict[str, list[str]] = {}
    for va in external:
        key = "0x%08X..0x%08X" % (va & ~0xFFF, (va & ~0xFFF) + 0xFFF)
        groups.setdefault(key, []).append("0x%08X" % va)
    # Privacy/copyright-conscious 16-byte context fingerprints rather than
    # large excerpts of the original executable.
    fingerprints = {}
    for va in external:
        # The file-backed bounds have already been asserted by the PE parser.
        context = pe.read(va - 8, 21)
        fingerprints["0x%08X" % va] = hashlib.sha256(context).hexdigest()[:16]
    return {
        "identity": {
            "sha256": verified["sha256"],
            "bytes": verified["bytes"],
            "scope": "original static PE32 call references; no runtime mode inference",
        },
        "direct_calls": {
            "0x%08X" % target: {
                "target_role": {
                    0x0050A190: "keyboard/joystick semantic mask aggregator",
                    0x00505720: "shared input service; increments 0x0066129C",
                    0x00479540: "per-action 16-byte state latch updater",
                    0x004796A0: "post-update state routine",
                }[target],
                "count": len(addresses),
                "callsites": ["0x%08X" % va for va in addresses],
            } for target, addresses in found.items()
        },
        "script_dispatch_structural_evidence": {
            "verified_instruction_sites": script_proofs,
            "caller": "0x004E0461",
            "script_handler": "0x00427910",
            "number_of_verified_calls": len(script_inline)+len(script_helper),
            "inline_calls": ["0x%08X" % va for va in script_inline],
            "helper_entry": "0x00427E40",
            "helper_xrefs_from_dispatch": ["0x%08X" % x for x in helper_xrefs],
            "helper_calls": ["0x%08X" % va for va in script_helper],
            "callsites": ["0x%08X" % va for va in script_inline+script_helper],
            "other_calls_not_yet_classified": len(external)-len(script_inline)-len(script_helper),
            "game_mode": "UNKNOWN: script commands may execute in different modes",
            "input_gate_reset": "0x0050C180 may call 0x0050A9B0 after accepting directional events",
        },
        "shared_service_calls_by_4kb_address_bucket": groups,
        "shared_service_callsite_context_sha256_16": fingerprints,
        "static_invariants": {
            "aggregator_only_caller": "0x00505738",
            "state_latch_updater_calls_all_inside": "0x00505720..0x00505902",
            "service_called_from_many_code_regions": len(groups) > 5,
            "function_context": "UNKNOWN until sampled per native menu/3D/Bink",
            "hook_authorized": False,
            "bridge_enabled": False,
        },
        "interpretation": (
            "The 56 static direct calls span multiple code regions; this is "
            "evidence against assuming the input service is 3D-only. "
            "All callsites must be classified using native game context "
            "before a generic SDL3 owner can replace physical joystick input."
        ),
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path, help="unmodified exact retail SpideyPC.exe")
    args = parser.parse_args(argv)
    try:
        if args.exe.stat().st_size != RETAIL_SIZE:
            raise InvalidExecutable("unsupported size: original EXE required")
        data = args.exe.read_bytes()
        if hashlib.sha256(data).hexdigest() != RETAIL_SHA:
            raise InvalidExecutable("unsupported SHA-256: original EXE required")
        report = audit(data)
    except (OSError, InvalidExecutable) as error:
        print("ERROR: %s" % error, file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
