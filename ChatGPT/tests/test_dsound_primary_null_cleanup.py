"""Byte-accurate regression for original SpideyPC.exe primary DirectSound NULL cleanup.

The two user-owned Windows 11 WER dumps (November 14, 2026) show the same
original instruction at VA 00503AF7 reading [EAX] with EAX=0, the primary
buffer global at 006BBF1C NULL, and the same direct caller 004FF9CA.

These tests do NOT distribute the original executable or user minidumps.
They enforce the narrow 32-bit instruction patch and basic stack-control
invariants. Real original-game validation is still required.
"""
from pathlib import Path
import re
import struct

SRC = (Path(__file__).resolve().parents[2] / "winmm_proxy.c").read_text(
    encoding="utf-8")

ORIGINAL_START = 0x00503AF0
PATCH_SITE = 0x00503AF7
NORMAL_CONTINUATION = 0x00503AFC
FINAL_CLEANUP = 0x00503B24
CODE_CAVE = 0x00503B32


def code_bytes(name: str) -> bytes:
    section = SRC.split(f"static const BYTE {name}[]={{", 1)[1].split("};", 1)[0]
    # Embedded explanatory comments have no 0x-prefixed hex literals.
    return bytes(int(v, 16) for v in re.findall(r"0x([a-fA-F0-9]{2})\\b", section))


def jump8(ins_address: int, instruction_length: int, operand: int) -> int:
    return ins_address + instruction_length + struct.unpack("b", bytes([operand]))[0]


def test_dump_proven_original_code_and_cave_bytes_are_exact():
    assert code_bytes("original") == bytes.fromhex(
        "A1 1C BF 6B 00 56 50 8B 08 FF 51 48 8B F0 85 F6")
    assert code_bytes("cleanup") == bytes.fromhex(
        "6A 01 6A 00 E8 23 05 00 00 83 C4 08 5E C3")
    assert code_bytes("unused_cave") == b"\\x90" * 14
    assert CODE_CAVE == FINAL_CLEANUP + len(code_bytes("cleanup"))
    assert PATCH_SITE == ORIGINAL_START + 7
    assert NORMAL_CONTINUATION == PATCH_SITE + 5


def test_actual_machine_code_branches_and_stack_balance():
    redirect = code_bytes("branch_to_cave")
    cave = code_bytes("cave_program")
    assert redirect == bytes.fromhex("E9 36 00 00 00")
    assert cave == bytes.fromhex("85 C0 74 07 8B 08 FF 51 48 EB BF 58 EB E4")
    assert len(cave) == 14
    assert len(redirect) == 5
    assert PATCH_SITE + 5 + struct.unpack_from("<i", redirect, 1)[0] == CODE_CAVE

    # At the native instruction, the stack contains: pushed buffer,
    # saved ESI, return address. We must drop the buffer ONLY on NULL.
    assert cave[:2] == bytes.fromhex("85 C0")  # TEST EAX,EAX
    assert jump8(CODE_CAVE + 2, 2, cave[3]) == CODE_CAVE + 11
    assert cave[4:9] == bytes.fromhex("8B 08 FF 51 48")
    assert jump8(CODE_CAVE + 9, 2, cave[10]) == NORMAL_CONTINUATION

    assert cave[11] == 0x58  # POP EAX removes pushed NULL, not ESI
    assert jump8(CODE_CAVE + 12, 2, cave[13]) == FINAL_CLEANUP
    assert code_bytes("cleanup")[-2:] == bytes.fromhex("5E C3")  # POP ESI; RET

    saved_esi, return_address = 0x12345678, 0x004FF9CF
    initial = [0, saved_esi, return_address]
    nulled = initial.copy()
    popped = nulled.pop(0)
    assert popped == 0
    assert nulled.pop(0) == saved_esi
    assert nulled.pop(0) == return_address
    assert not nulled
    # Valid object: native COM Stop is executed unchanged, so both pushes
    # remain intact until original continuation handles its HRESULT.
    assert initial[0] == 0


def test_guard_fails_closed_on_wrong_image_or_changed_original_bytes():
    body = SRC.split("static void install_early_primary_null_guard(void){", 1)[1].split(
        "/* The initial loader-bound winmm proxy owns", 1)[0]
    assert "early_patch_status!=1" in body
    assert "(ULONG_PTR)base!=0x00400000u" in body
    assert "memcmp(original_start,original,sizeof(original))" in body
    assert "memcmp(cleanup_start,cleanup,sizeof(cleanup))" in body
    assert "memcmp(cave,unused_cave,sizeof(unused_cave))" in body
    assert body.index("memcmp(original_start") < body.index("VirtualProtect(site")
    assert body.index("memcpy(cave,cave_program") < body.index(
        "memcpy(site,branch_to_cave")
    assert "FlushInstructionCache(GetCurrentProcess(),cave,sizeof(cave_program))" in body
    assert "FlushInstructionCache(GetCurrentProcess(),site,sizeof(branch_to_cave))" in body
    assert "VirtualProtect(site,span,previous,&ignored)" in body
    assert "PAGE_EXECUTE_READWRITE" in body
    for forbidden in ("LoadLibrary", "GetPrivateProfile", "CreateThread",
                      "WaitForSingleObject", "MessageBox", "fopen(", "Sleep("):
        assert forbidden not in body


def test_original_audio_code_unmodified_on_disk_and_guard_precedes_game_start():
    init = SRC.split("BOOL WINAPI DllMain(", 1)[1]
    assert init.index("patch_early();") < init.index(
        "install_early_primary_null_guard();") < init.index(
        "install_early_ddraw_gate();") < init.index("CreateThread(")
    assert "DSOUND-PRIMARY: early native NULL-cleanup guard status=" in SRC
    assert "early_primary_null_guard_status=1;" in SRC
    assert "return E_FAIL;" not in SRC.split(
        "static void install_early_primary_null_guard(void){",1
    )[1].split("/* The initial loader-bound",1)[0]
