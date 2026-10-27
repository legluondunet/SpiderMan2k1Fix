"""Original native WAV file last-partial-sector fix: static and arithmetic regression.

These tests validate the independently CRC-backed, bounded design but NOT
the behavior of the user-supplied SpideyPC.exe in a running game.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "pkr_audio_loader.c").read_text(encoding="utf-8")
MATCH = (ROOT / "pkr_audio_index.c").read_text(encoding="utf-8")
MAIN = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
INI = (ROOT / "ini_validation.h").read_text(encoding="utf-8")
BUILD = (ROOT / "build.sh").read_text(encoding="utf-8")


def test_loader_rootfix_explicit_off_default_and_exact_game_code_only():
    assert '"ExperimentalAudioLoaderTail"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioLoaderTail",0,ini)==1' in MAIN
    assert "SpiderPKRAudioLoader_Init(log_line,pkr_loader_tail);" in MAIN
    assert 'if(enabled!=1)return;' in SRC
    assert 'GetModuleHandleA(NULL)!=GAME_BASE' in SRC
    assert "0x00503CB4u" in SRC
    assert "0x00503CD8u" in SRC
    assert "0x00505170u" in SRC
    assert "0x005051D0u" in SRC
    assert "0xE8,0xB7,0x14,0x00,0x00" in SRC
    assert "0xE8,0xF3,0x14,0x00,0x00" in SRC
    assert "memcmp(a,expected_length_call,5u)!=0" in SRC
    assert "memcmp(b,expected_sector_call,5u)!=0" in SRC
    assert "pkr_audio_loader.c" in BUILD
    assert MAIN.index("SpiderPKRAudioIndex_Init(log_line") < MAIN.index("SpiderPKRAudioLoader_Init(log_line")


def test_loader_only_writes_exact_original_tail_after_fresh_native_read_validation():
    assert "result=ORIGINAL_SECTOR_READ(handle,blocks,dst);" in SRC
    assert "if(!installed||result!=0||!dst" in SRC
    assert "blocks!=(length>>11)" in SRC
    assert "complete=blocks*2048u;" in SRC
    assert "if(complete>=length)return result;" in SRC
    assert "remainder=length-complete;" in SRC
    assert "if(!remainder||remainder>=2048u)return result;" in SRC
    assert "SpiderPKRAudioIndex_ExpectedLoaderWavTail(" in SRC
    assert "unsigned char scratch[2048];" in SRC
    assert "extra=ORIGINAL_SECTOR_READ(handle,1u,scratch);" in SRC
    assert "memcmp(scratch,expected,remainder)!=0" in SRC
    assert "memcpy((unsigned char*)dst+complete,scratch,remainder);" in SRC
    assert SRC.index("result=ORIGINAL_SECTOR_READ(handle,blocks,dst);") < SRC.index(
        "SpiderPKRAudioIndex_ExpectedLoaderWavTail(") < SRC.index(
        "extra=ORIGINAL_SECTOR_READ(handle,1u,scratch);") < SRC.index(
        "memcpy((unsigned char*)dst+complete,scratch,remainder);")
    assert "TlsAlloc()" in SRC and "TlsSetValue" in SRC and "TlsGetValue" in SRC
    assert "staged_handle!=(uint32_t)handle" in SRC
    assert "length>MAX_WAV_BYTES" in SRC
    assert "LOADER-TAIL-RESTORED" not in SRC  # logged through LOADER-%s formatter
    assert 'loader_message("TAIL-RESTORED"' in SRC


def test_source_requires_unique_verified_44byte_riff_and_pcm_prefix():
    assert "if(!index_ready||!wav" in MATCH
    assert "complete_bytes!=(total_bytes&~2047u)" in MATCH
    assert 'memcmp(wav,"RIFF",4)!=0' in MATCH
    assert 'memcmp(wav+8,"WAVE",4)!=0' in MATCH
    assert 'memcmp(wav+12,"fmt ",4)!=0' in MATCH
    assert 'memcmp(wav+36,"data",4)!=0' in MATCH
    assert "le32(wav+4)!=total_bytes-8u" in MATCH
    assert "w->header_bytes!=44u" in MATCH
    assert "w->pcm_bytes!=total_bytes-44u" in MATCH
    assert "memcmp(wav+44u,w->pcm,pcm_prefix)!=0" in MATCH
    assert "if(++matches>1u)return NULL" in MATCH
    assert "return match->pcm+pcm_prefix" in MATCH
    assert "crc32_pkraw(raw,e->length)==e->crc" in MATCH


def test_arithmetic_does_not_round_up_or_overrun_native_malloc():
    examples=[
        ("Heli1.wav",60524,1132),
        ("ThugHit2.wav",15182,846),
        ("traffic2.wav",79350,1526),
        ("exactly one block",2048,0),
        ("zero-length",0,0),
        ("first block + one",2049,1),
        ("just before second block",4095,2047),
    ]
    for name,total,expected_remainder in examples:
        full=(total//2048)*2048
        rem=total-full
        assert rem==expected_remainder,(name,total,full,rem)
        assert full+rem==total
        assert 0<=rem<2048
        # Dedicated scratch is 2048; destination write stays within the
        # game's original exact-length allocation rather than ceil(length).
        scratch=bytearray(2048)
        destination=bytearray(total)
        if rem:
            destination[full:full+rem]=scratch[:rem]
        assert len(destination)==total


def test_native_rootfix_keeps_fallback_and_does_not_replace_directsound():
    assert "SpiderPKRAudioIndex_TryTailRestore" in (ROOT/"pkr_audio_play_hook.c").read_text()
    assert "SpiderPKRAudioIndex_TryHeli1Declick" in (ROOT/"pkr_audio_play_hook.c").read_text()
    for forbidden in ("CreateSoundBuffer", "DuplicateSoundBuffer", "IDirectSoundBuffer_Lock",
                      "IDirectSoundBuffer_Unlock", "stop_all_voices", "SDL3", "XAudio2"):
        assert forbidden not in SRC
    assert "VirtualProtect" in SRC
    assert "FlushInstructionCache" in SRC
    assert "memcpy(a,expected_length_call,5u)" in SRC
    assert "memcpy(b,expected_sector_call,5u)" in SRC
