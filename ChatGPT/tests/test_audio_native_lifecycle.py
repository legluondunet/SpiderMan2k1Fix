"""Opt-in native game-owned voice Play/Stop probe.

No changes to system DirectSound, buffer content, playback frequency,
sample volume, or original call argument conventions.
"""
from pathlib import Path
import hashlib, os, struct
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"audio_native_lifecycle.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()
BUILD=(ROOT/"build.sh").read_text()

ORIGINAL_SITES={
    0x4722F3:"e8 38 1f 09 00",
    0x472870:"e8 bb 19 09 00",
    0x4723F2:"e8 a9 1e 09 00",
    0x4725EE:"e8 ad 1c 09 00",
    0x47261F:"e8 7c 1c 09 00",
    0x4729C4:"e8 d7 18 09 00",
}

def test_exact_six_original_callsites_and_read_only_forwarders():
    assert "NATIVE_HOOK_COUNT" in SRC
    assert "memcmp(native_sites[i].call_site,native_sites[i].expected,5u)" in SRC
    for va,data in ORIGINAL_SITES.items():
        assert f"0x{va:08X}u" in SRC
        assert "{"+",".join(f"0x{b:02X}" for b in bytes.fromhex(data))+"}" in SRC
    assert "0x00504230u" in SRC
    assert "0x005042A0u" in SRC
    assert "SPIDER_NATIVE_PLAY(slot,looping);" in SRC
    assert "SPIDER_NATIVE_STOP(slot);" in SRC
    assert "void __cdecl native_play_oneshot(" in SRC
    assert "void __cdecl native_play_loop(" in SRC
    assert "void __cdecl native_stop_actor(" in SRC
    assert "void __cdecl native_stop_recycle_first(" in SRC
    assert "void __cdecl native_stop_recycle_second(" in SRC
    assert "void __cdecl native_stop_teardown(" in SRC
    for forbidden in ("IDirectSoundBuffer_Stop(","IDirectSoundBuffer_Play(",
                      "IDirectSoundBuffer_SetFrequency(","IDirectSoundBuffer_SetVolume(",
                      "IDirectSoundBuffer_Lock(","IDirectSoundBuffer_SetCurrentPosition("):
        assert forbidden not in SRC


def test_gated_no_per_frame_com_or_extra_mutation():
    assert 'ExperimentalAudioNativeLifecycleTrace",0,path)!=1' in SRC
    assert '"ExperimentalAudioNativeLifecycleTrace"' in VALID
    assert "GetModuleHandleA(NULL)!=SPIDER_NATIVE_BASE" in SRC
    assert "VirtualProtect(page,4096u,PAGE_EXECUTE_READWRITE,&oldprotection)" in SRC
    assert "VirtualProtect(page,4096u,oldprotection,&ignored)" in SRC
    assert "FlushInstructionCache(" in SRC
    assert "AUDIO-NATIVE: REFUSED" in SRC
    assert "AUDIO-NATIVE: ARMED" in SRC
    assert "900u" in SRC
    assert "SpiderNativeAudioTrace_OnFlip(" in CORE
    assert "SpiderNativeAudioTrace_Init(log_line);" in CORE
    assert "audio_native_lifecycle.c" in BUILD
    assert CORE.index("if (!supported_exe())")<CORE.index("SpiderNativeAudioTrace_Init(log_line);")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);")<CORE.index("SpiderNativeAudioTrace_Init(log_line);")
    f=SRC[SRC.index("void SpiderNativeAudioTrace_OnFlip("):]
    for forbidden in ("capture_voice(","GetStatus","GetCurrentPosition",
                      "VirtualProtect(","SPIDER_NATIVE_STOP(","SPIDER_NATIVE_PLAY("):
        assert forbidden not in f


def test_native_sites_against_known_original_game_image_if_provided():
    exe=os.getenv("SPIDERMAN2K1FIX_TEST_EXE")
    if not exe:return
    binary=Path(exe).read_bytes()
    assert hashlib.sha256(binary).hexdigest()=="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
    p=struct.unpack_from("<I",binary,0x3c)[0]
    count=struct.unpack_from("<H",binary,p+6)[0]
    opt_size=struct.unpack_from("<H",binary,p+20)[0]
    for va,text in ORIGINAL_SITES.items():
        rva=va-0x400000
        match=None
        for j in range(count):
            sec=p+24+opt_size+40*j
            virtual_size,virtual_start,raw_size,raw_start=struct.unpack_from("<IIII",binary,sec+8)
            if virtual_start<=rva<virtual_start+min(virtual_size,raw_size):
                match=binary[raw_start+rva-virtual_start:raw_start+rva-virtual_start+5]
                break
        assert match==bytes.fromhex(text)
