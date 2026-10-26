"""Read-only PKR3 WAV index: guard against unsafe archive access and unsanctioned audio replacement."""
from pathlib import Path
import struct
import zlib

R=Path(__file__).resolve().parents[2]
SRC=(R/"pkr_audio_index.c").read_text()
HDR=(R/"pkr_audio_index.h").read_text()
MAIN=(R/"spiderman2k1fix.c").read_text()
PLAY=(R/"sdl3_swing5_redirect.c").read_text()
INI=(R/"ini_validation.h").read_text()
BUILD=(R/"build.sh").read_text()

def test_optin_and_readonly_sound_pipeline():
    assert '"ExperimentalSDL3PKRAudioIndex"' in INI
    assert 'GetPrivateProfileIntA("Debug","ExperimentalSDL3PKRAudioIndex",0,ini)==1' in MAIN
    assert 'SpiderPKRAudioIndex_Init(log_line,pkr_readonly)' in MAIN
    assert 'SpiderSDL3Swing5_Init(log_line,swing||pkr_readonly)' in MAIN
    assert 'SpiderPKRAudioIndex_Probe(slot,buf,looping)' in PLAY
    assert 'pkr_audio_index.c' in BUILD
    assert 'third_party/miniz/miniz_tinfl.c' in BUILD
    assert 'SpiderPKRAudioIndex_Init' in HDR
    assert 'SpiderPKRAudioIndex_Probe' in HDR
    assert 'fopen(path,"rb")' in SRC
    assert 'record_is_valid(e,footer)' in SRC
    assert 'crc32_pkraw(raw,e->length)==e->crc' in SRC
    assert 'TINFL_FLAG_PARSE_ZLIB_HEADER' in SRC
    assert 'MAX_PCM_CACHE (128u*1024u*1024u)' in SRC
    assert 'MAX_PKR_FILES 16000u' in SRC
    assert 'MAX_WAV_COUNT 2048u' in SRC
    assert 'MAX_PKR_ITEM (16u*1024u*1024u)' in SRC
    assert 'memcmp(wav,"RIFF",4)' in SRC
    assert 'memcmp(wav+8,"WAVE",4)' in SRC
    assert 'miniz' in SRC.lower()
    assert 'PKR-AUDIO: MATCH' not in SRC  # log format chooses MATCH/AMBIGUOUS
    for forbidden in ('fopen(path,"wb")','fopen(path,"rb+")','fwrite(',
                      'WriteFile(', 'IDirectSoundBuffer_Play(', 
                      'VirtualProtect(', 'IDirectSoundBuffer_Stop(',
                      'SpiderSDL3Probe_PlaySwing5('):
        assert forbidden not in SRC

def test_pkr_layout_and_boundary_examples():
    assert struct.calcsize("<4sI")==8
    assert struct.calcsize("<III")==12
    assert struct.calcsize("<32sII")==40
    assert struct.calcsize("<32sIiIII")==52
    assert '#define PKR_DIR_BYTES 40u' in SRC
    assert '#define PKR_FILE_BYTES 52u' in SRC
    assert '#define MIN_MATCH_BYTES 1024u' in SRC
    assert 'e->stored>footer-e->offset' in SRC
    assert 'e->length!=e->stored' in SRC
    assert 'fmt.wFormatTag!=WAVE_FORMAT_PCM' in SRC
    assert 'fmt.nChannels!=1u' in SRC
    assert 'fmt.wBitsPerSample!=16u' in SRC
    assert 'caps.dwBufferBytes==sources[i].pcm_bytes' not in SRC
    assert 'sources[i].pcm_bytes==caps.dwBufferBytes' in SRC
    assert 'sources[i].rate==fmt.nSamplesPerSec' in SRC
    assert 'memcmp(w->pcm,first,size1)' in SRC
    assert 'memcmp(w->pcm+size1,second,size2)' in SRC
    assert 'IDirectSoundBuffer_Unlock' in SRC

def test_original_crc_and_deflate_are_separate_from_game_pcm():
    raw=(b'RIFF'+struct.pack('<I',36+8192)+b'WAVE'
         +b'fmt '+struct.pack('<IHHIIHH',16,1,1,16000,32000,2,16)
         +b'data'+struct.pack('<I',8192)+bytes(range(256))*32)
    assert len(raw)==44+8192
    compressed=zlib.compress(raw)
    assert zlib.decompress(compressed)==raw
    assert zlib.crc32(raw)!=zlib.crc32(raw[:-512]+bytes(512))
    assert SRC.index('crc32_pkraw(raw,e->length)==e->crc') < SRC.index('memcpy(dst->pcm,pcm,pcm_bytes)')
    assert SRC.index('SpiderPKRAudioIndex_Probe') > SRC.index('SpiderPKRAudioIndex_Init')
