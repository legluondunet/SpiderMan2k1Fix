"""Opt-in native Win11 DirectInput8 DIERR_NOTACQUIRED recovery guard."""
from pathlib import Path
import hashlib
import os
import struct

ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"input_notacquired_retry.c").read_text()
MAIN=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
DOC=(ROOT/"docs/references/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()
ORIGINAL_SHA256="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"

def test_opt_in_and_exact_game_guard():
    assert 'SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY' in SRC  # backward-compatible override
    assert 'GetPrivateProfileIntA("Input","DirectInputReacquire",0,ini)==1' in SRC
    assert "enabled_env=(env_length==1 && value[0]=='1');" in SRC
    assert "InterlockedExchange(&requested,(enabled_ini || enabled_env)?1:0)" in SRC
    assert "source=%s%s" in SRC
    assert MAIN.index("SpiderCfg_ValidateCurrentIni(log_line);") < MAIN.index("SpiderDI8Retry_Init(log_line);")
    assert 'sizeof(void*)!=4' in SRC
    assert 'SPIDER_IMAGE_BASE 0x00400000u' in SRC
    assert 'memcmp((void *)(uintptr_t)SPIDER_KEY_GETDATA' in SRC
    assert 'memcmp((void *)(uintptr_t)SPIDER_MOUSE_GETDATA' in SRC
    assert MAIN.index("if (!supported_exe())") < MAIN.index("SpiderDI8Retry_Init(log_line);")
    assert 'SpiderDI8Retry_OnFlip(count);' in MAIN
    assert "input_notacquired_retry.c" in BUILD

def test_branches_and_safety_constraints():
    assert 'SPIDER_KEY_JNE 0x00501BCBu' in SRC
    assert 'SPIDER_MOUSE_JNE 0x00501D0Au' in SRC
    assert 'SPIDER_KEY_ACQUIRE 0x00501BD1u' in SRC
    assert 'SPIDER_MOUSE_ACQUIRE 0x00501D10u' in SRC
    assert 'SPIDER_KEY_NORMAL 0x00501C51u' in SRC
    assert 'SPIDER_MOUSE_NORMAL 0x00501D90u' in SRC
    assert 'SPIDER_DIERR_NOTACQUIRED 0x8007000Cu' in SRC
    assert 'SPIDER_DIERR_INPUTLOST 0x8007001Eu' in SRC
    assert 'emit_cmp_eax(&p,SPIDER_DIERR_INPUTLOST)' in SRC
    assert 'emit_cmp_eax(&p,SPIDER_DIERR_NOTACQUIRED)' in SRC
    assert 'SPIDER_GAME_ACTIVE 0x006B78F0u' in SRC
    assert 'if(flip_index==120) try_install();' in SRC
    assert 'original_jne' in SRC
    assert 'restore_original_jne(' in SRC
    assert 'if(restore_original_jne(' in SRC
    assert 'PAGE_EXECUTE_READ' in SRC
    assert 'VirtualFree(page,0,MEM_RELEASE)' in SRC

def test_old_stack_relative_branch_error_not_reintroduced():
    # The withdrawn experiment encoded JMP offsets inside a temporary local
    # BYTE jump[6] and copied them to the EXE, crashing before first Flip.
    # The site-relative subtraction MUST use the original EXE instruction VA.
    assert 'jump_rel32=(uint32_t)(uintptr_t)destination-' in SRC
    assert '((uint32_t)(uintptr_t)where+5u)' in SRC
    assert 'memcpy(patch+1,&jump_rel32,4)' in SRC
    assert 'rel32(disp,destination)' in SRC
    assert 'from=(uint32_t)(uintptr_t)field+4u;' in SRC
    for site, acquire, normal in ((0x501BCB,0x501BD1,0x501C51),
                                 (0x501D0A,0x501D10,0x501D90)):
        assert struct.unpack("<I",struct.pack("<I",(acquire-(site+5))&0xffffffff))[0] == 1
        assert normal-(site+6)==0x80

def test_optional_game_binary_matches_checked_original_code():
    # GitHub CI does not ship the copyrighted EXE. A developer can run with
    # SPIDERMAN2K1FIX_TEST_EXE=/path/to/SpideyPC.exe to verify the signatures.
    path=os.environ.get("SPIDERMAN2K1FIX_TEST_EXE")
    if not path:
        return
    exe=Path(path).read_bytes()
    assert hashlib.sha256(exe).hexdigest()==ORIGINAL_SHA256
    pe=struct.unpack_from("<I",exe,0x3c)[0]
    sections=struct.unpack_from("<H",exe,pe+6)[0]
    opt_size=struct.unpack_from("<H",exe,pe+20)[0]
    sig=bytes.fromhex("ff 51 28 3d 1e 00 07 80 0f 85 80 00 00 00")
    for va in (0x501BC3,0x501D02):
        rva=va-0x400000
        matched=False
        for i in range(sections):
            sh=pe+24+opt_size+i*40
            vsize,section_rva,rawsize,offset=struct.unpack_from("<IIII",exe,sh+8)
            if section_rva <= rva < section_rva+min(vsize,rawsize):
                assert exe[offset+rva-section_rva:offset+rva-section_rva+14]==sig
                matched=True
                break
        assert matched

def test_documented_mode():
    assert '$env:SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY' in DOC
    assert 'WinDbg' in DOC
