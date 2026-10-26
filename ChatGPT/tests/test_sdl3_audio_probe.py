"""SDL3 F7 isolated prototype: guardrails against audio takeover."""
from pathlib import Path
R=Path(__file__).resolve().parents[2]
S=(R/"sdl3_audio_probe.c").read_text()
H=(R/"sdl3_audio_probe.h").read_text()
N=(R/"spiderman2k1fix.c").read_text()
I=(R/"ini_validation.h").read_text()
B=(R/"build.sh").read_text()
W=(R/".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
def test_optin_bounded_source_and_original_game_remains_untouched():
    assert '"ExperimentalSDL3AudioProbe"' in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalSDL3AudioProbe",0,ini)==1' in N
    assert "SpiderSDL3Probe_Init(log_line,test,swing)" in N
    assert "SpiderSDL3Probe_OnFlip(game_hwnd)" in N
    assert "GetForegroundWindow()!=game" in S
    assert "GetAsyncKeyState(VK_F7)" in S
    assert "SpiderMan2k1Fix-SDL3-test.wav" in S
    assert "MAX_PCM_BYTES (16u*1024u*1024u)" in S
    assert "LoadLibraryA(path)" in S
    assert "SDL_LoadWAV" in S
    assert "SDL_OpenAudioDeviceStream" in S
    assert "SDL_PutAudioStreamData" in S
    assert "SDL_ResumeAudioStreamDevice" in S
    assert "SDL_DestroyAudioStream" in S
    for prohibited in ("DirectSoundCreate(", "IDirectSoundBuffer_Play(",
                       "IDirectSoundBuffer_Stop(", "VirtualProtect(",
                       "PKR_ReadFile("):
        assert prohibited not in S
    assert "sdl3_audio_probe.c" in B
    assert "sha256sum -c -" in W
    assert "SDL3-3.4.18-win32-x86.zip" in W
