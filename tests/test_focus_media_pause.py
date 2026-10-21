"""Experimental native Bink pause and passive DirectSound diagnostics."""
from pathlib import Path
import os,struct,hashlib

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"focus_media_pause.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()

def test_bink_sync_requires_explicit_opt_in():
    assert "PauseBinkOnFocusLoss=0" in INI
    assert '"PauseBinkOnFocusLoss=0\\r\\n"' in GEN
    assert '"PauseBinkOnFocusLoss"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseBinkOnFocusLoss",0,exe)!=1' in SRC
    assert "focus_media_pause.c" in BUILD

def test_native_bink_call_requires_original_supported_layout():
    assert '0x0050BDE5u' in SRC
    assert '0x0053B210u' in SRC
    assert '0x00AC0BA4u' in SRC
    assert '0x00AC1020u' in SRC
    assert 'GetModuleHandleA("binkw32.dll")' in SRC
    assert 'GetProcAddress(bink,"_BinkPause@8")' in SRC
    assert "memcmp(SPIDER_BINK_CALLSITE,bink_signature,sizeof(bink_signature))" in SRC
    assert '0xFF,0x15,0x10,0xB2,0x53,0x00' in SRC
    assert "imported!=(void *)exported" in SRC
    assert "secondary DirectSound voice handling is separate" in SRC

def test_media_pause_lifecycle_and_no_unsafe_hooks():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.index("SpiderFocusMedia_Pause();") < pause.index("while(IsWindow(hwnd)")
    assert pause.count("SpiderFocusMedia_Resume();")==2
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderFocusMedia_Init(log_line);")
    assert "native_bink_pause(handle,1)" in SRC
    assert "native_bink_pause(paused[i],0)" in SRC
    assert "now[i]==paused[i]" in SRC
    assert 'SPIDER_BINK_GAME_PAUSE' in SRC
    for word in ("VirtualProtect", "SetWindowsHookEx", "CreateDevice", "SuspendThread", "Stop()", "SetVolume"):
        assert word not in SRC

def test_audio_probe_has_no_directsound_mutation():
    assert "FOCUS-AUDIO:" in SRC
    assert "no sound-buffer changes" in SRC
    assert "0x006B7920u" in SRC and "0x006B7924u" in SRC

def test_optional_original_exe_bink_signature():
    src=os.getenv("SPIDERMAN2K1FIX_TEST_EXE")
    if not src:return
    exe=Path(src).read_bytes()
    assert hashlib.sha256(exe).hexdigest()=="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
    pe=struct.unpack_from("<I",exe,0x3C)[0]
    opt=struct.unpack_from("<H",exe,pe+20)[0]
    sections=struct.unpack_from("<H",exe,pe+6)[0]
    rva=0x50BDE5-0x400000
    for i in range(sections):
        off=pe+24+opt+i*40
        vsize,s_rva,rawsize,foff=struct.unpack_from("<IIII",exe,off+8)
        if s_rva<=rva<s_rva+min(vsize,rawsize):
            assert exe[foff+rva-s_rva:foff+rva-s_rva+6]==bytes.fromhex("FF 15 10 B2 53 00")
            return
    assert False,"Bink native pause callsite was not mapped"
