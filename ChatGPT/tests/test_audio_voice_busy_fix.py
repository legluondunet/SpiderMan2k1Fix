"""Regression guard for an opt-in, one-byte *in-memory* original game repair.

No Windows executable is included in the repo. Set SPIDERMAN2K1FIX_TEST_EXE
to validate byte signatures against the exact SHA-256-supported game image.
"""
from pathlib import Path
import hashlib, os
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"audio_voice_busy_fix.c").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
BUILD=(ROOT/"build.sh").read_text()
VALID=(ROOT/"ini_validation.h").read_text()


def test_voice_status_occupancy_fix_is_opt_in_and_exact():
    assert 'ExperimentalAudioVoiceBusyFix",0,path)!=1' in SRC
    assert "0x0050448Fu" in SRC
    assert "0x00504488u" in SRC
    assert "0x8B,0x44,0x24,0x04,0x5E,0x83,0xE0,0x04,0x59,0xC3" in SRC.replace("\n","").replace(" ","")
    assert "*VOICE_FLAG_VA=0x01u;" in SRC
    assert "*VOICE_FLAG_VA=0x04u;" in SRC
    assert "memcmp(VOICE_TEST_VA,original_status_tail" in SRC
    assert "VirtualProtect(VOICE_TEST_VA" in SRC
    assert "FlushInstructionCache(" in SRC
    assert "GetModuleHandleA(NULL)" in SRC
    assert "AUDIO-VOICE-BUSY: APPLIED" in SRC
    assert "AUDIO-VOICE-BUSY: REFUSED" in SRC
    assert "ExperimentalAudioVoiceBusyFix" in VALID


def test_voice_fix_is_early_and_after_exe_sha_and_ini_validation():
    assert "audio_voice_busy_fix.c" in BUILD
    assert '#include "audio_voice_busy_fix.h"' in CORE
    assert "SpiderVoiceBusyFix_Init(log_line);" in CORE
    assert CORE.index("if (!supported_exe())") < CORE.index("SpiderVoiceBusyFix_Init(log_line);")
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line);") < CORE.index("SpiderVoiceBusyFix_Init(log_line);")
    # No hook/COM rewrites or repeated per-Flip checks; only the exact byte.
    for forbidden in ("IDirectSoundBuffer_Stop","IDirectSoundBuffer_Play",
                      "IDirectSoundBuffer_SetFrequency","IDirectSoundBuffer_SetVolume",
                      "SetWindowsHookEx", "SetCurrentPosition", "CreateSoundBuffer"):
        assert forbidden not in SRC


def test_status_semantics_not_confused():
    # DirectSound documented DSBSTATUS_PLAYING=0x1, DSBSTATUS_LOOPING=0x4.
    one_shot_playing=0x1
    looping_playing=0x5
    one_shot_stopped=0x0
    assert (one_shot_playing & 0x4)==0  # Original incorrectly: free
    assert (one_shot_playing & 0x1)!=0   # Correct: occupied
    assert (looping_playing & 0x1)!=0
    assert (one_shot_stopped & 0x1)==0


def test_known_supported_exe_instructions_if_supplied():
    filename=os.getenv("SPIDERMAN2K1FIX_TEST_EXE")
    if not filename:return
    exe=Path(filename).read_bytes()
    assert hashlib.sha256(exe).hexdigest()=="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
    # This original PE has .text VirtualAddress=0x1000 and raw=0x1000.
    assert exe[0x104488:0x104492]==bytes.fromhex("8b 44 24 04 5e 83 e0 04 59 c3")
