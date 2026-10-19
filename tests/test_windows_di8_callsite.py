"""Source/EXE regression for native DirectInput polling error branches."""
from pathlib import Path
import hashlib, struct

ROOT=Path(__file__).resolve().parents[1]
S=(ROOT/"input_callsite_guard.c").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
DOC=(ROOT/"WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_guards_and_no_com_vtable_changes():
    assert "SPIDERMAN2K1FIX_DI8_POLL_TRACE" in S
    assert "SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY" in S
    assert "memcmp(exe+KEY_BRANCH_RVA-5,expected_branch" in S
    assert "memcmp(exe+MOUSE_BRANCH_RVA-5,expected_branch" in S
    assert "if(!trace && !repair) return;" in S
    assert "DIERR_INPUTLOST_VALUE" in S
    assert "DIERR_NOTACQUIRED_VALUE" in S
    assert "0x00101BCBu" in S and "0x00101D0Au" in S
    assert "SPIDEY_EXE_BASE 0x00400000u" in S
    assert "original_jne" in S
    assert "AddVectoredExceptionHandler" not in S
    assert "DirectInput8Create" not in S.split("void SpiderDI8Callsite_Init",1)[-1]

def test_hooks_called_only_after_sha_check_and_on_flip():
    assert MAIN.index("if (!supported_exe())") < MAIN.index("SpiderDI8Callsite_Init(log_line)")
    assert "SpiderDI8Callsite_Report(count);" in MAIN
    assert "input_callsite_guard.c" in BUILD
    assert "input_callsite_guard.h" in MAIN

def test_two_error_conditions_are_kept():
    assert "emit_u32(&p,DIERR_NOTACQUIRED_VALUE)" in S
    assert "emit_u32(&p,DIERR_INPUTLOST_VALUE)" in S
    assert "if(allow_retry) emit_jmp(&p,acquire)" in S
    assert "emit_jmp(&p,continue_normal)" in S

def test_documented_modes():
    assert "$env:SPIDERMAN2K1FIX_DI8_POLL_TRACE='1'" in DOC
    assert "0x00501B80" in DOC and "0x00501CC0" in DOC
