"""Experimental game-owned DirectSound secondary-buffer pause.
No assertions here substitute for real Win11/ProcDump audio testing.
"""
from pathlib import Path
import hashlib,os,struct
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"focus_dsound_pause.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
EXAMPLE=(ROOT/"SpiderMan2k1Fix.generated-example.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()

def test_opt_in_config_is_disabled_by_default():
    for content in (INI,EXAMPLE):
        assert "PauseDirectSoundOnFocusLoss=0" in content
        assert "PauseBinkOnFocusLoss=0" in content
    assert '"PauseDirectSoundOnFocusLoss=0\\r\\n"' in GEN
    assert '"PauseDirectSoundOnFocusLoss"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseDirectSoundOnFocusLoss",0,path)!=1' in SRC
    assert "focus_dsound_pause.c" in BUILD

def test_layout_and_signatures():
    assert "VOICE_COUNT 32u" in SRC
    assert "0x006BBD50u" in SRC
    assert "slot*12u" in SRC
    assert "0x006BBF1Cu" in SRC
    assert "buf==SPIDER_PRIMARY" in SRC
    assert "0x005039F0u" in SRC and "0x00504110u" in SRC
    assert "0x005042A0u" in SRC
    assert "memcmp(SPIDER_VOICE_INIT" in SRC
    assert "memcmp(SPIDER_DUPLICATE" in SRC
    assert "memcmp(SPIDER_VOICE_STOP" in SRC

def test_pause_only_live_secondary_voices_and_restore_respected():
    assert "DSBSTATUS_PLAYING" in SRC
    assert "IDirectSoundBuffer_GetStatus(buf,&status)" in SRC
    assert "IDirectSoundBuffer_AddRef(buf);" in SRC
    assert "IDirectSoundBuffer_Stop(buf);" in SRC
    assert "paused_voice[i]=buf;" in SRC
    assert "DSBSTATUS_LOOPING" in SRC
    assert "DSBPLAY_LOOPING" in SRC
    assert "if(voice_at(i)==buf)" in SRC
    assert "IDirectSoundBuffer_Play(buf,0,0,paused_play_flags[i])" in SRC
    assert "IDirectSoundBuffer_Release(buf);" in SRC
    assert "DSBSTATUS_BUFFERLOST" in SRC
    for forbidden in ("VirtualProtect", "SetWindowsHookEx", "SetCurrentPosition",
                      "SetVolume", "GetDC", "SuspendThread", "CreateSoundBuffer"):
        assert forbidden not in SRC

def test_lifecycle_paired_and_after_exe_check_and_validated_ini():
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert pause.count("SpiderFocusDSound_Pause();")==1
    assert pause.count("SpiderFocusDSound_Resume();")==2
    assert pause.index("SpiderFocusDSound_Pause();") < pause.index("while(IsWindow(hwnd)")
    assert pause.index("SpiderFocusDSound_Resume();") < pause.index("SpiderFocusMedia_Resume();")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderFocusDSound_Init(log_line);")
    assert CORE.index("if (!supported_exe())") < CORE.index("SpiderFocusDSound_Init(log_line);")

def test_optional_original_exe_sound_table_signatures():
    path=os.getenv("SPIDERMAN2K1FIX_TEST_EXE")
    if not path:return
    exe=Path(path).read_bytes()
    assert hashlib.sha256(exe).hexdigest()=="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
    pe=struct.unpack_from("<I",exe,0x3c)[0]
    sections=struct.unpack_from("<H",exe,pe+6)[0]
    opt_size=struct.unpack_from("<H",exe,pe+20)[0]
    for va,sig in ((0x5039F0,bytes.fromhex("83 EC 24 56 57 B9 80 00 00 00")),
                   (0x504110,bytes.fromhex("8B 0D 20 79 6B 00 53 56 57")),
                   (0x5042A0,bytes.fromhex("8B 44 24 04 56 8D 04 40"))):
        rva=va-0x400000
        found=False
        for i in range(sections):
            sh=pe+24+opt_size+i*40
            vsize,sec_rva,rawsize,offset=struct.unpack_from("<IIII",exe,sh+8)
            if sec_rva<=rva<sec_rva+min(vsize,rawsize):
                assert exe[offset+rva-sec_rva:offset+rva-sec_rva+len(sig)]==sig
                found=True
                break
        assert found
