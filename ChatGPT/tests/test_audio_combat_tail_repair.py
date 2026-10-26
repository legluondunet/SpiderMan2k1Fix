"""Guardrails for 3 opt-in exact SHA-256 one-shot PCM suffix repairs.

This test never runs the Windows DLL or mutates a real game buffer.
"""
from pathlib import Path
import re

R = Path(__file__).resolve().parents[2]
S = (R / "audio_combat_tail_repair.c").read_text()
N = (R / "audio_native_lifecycle.c").read_text()
I = (R / "ini_validation.h").read_text()
B = (R / "build.sh").read_text()
H = (R / "audio_combat_tail_repair.h").read_text()

CASES = (
    ("R2-03", 29924, 16000, 14 * 2048 - 44, 240,
     "910fbf35919a16880f6b7783828b767ed92f28c1a8c37e5bef8200ff297fc892"),
    ("R2-07", 15140, 16000, 7 * 2048 - 44, 240,
     "f61d96ce26df86781b7dc8793db99dc9dcc834135a1b079052a56710cc18980c"),
    ("R1-08", 10128, 22050, 4 * 2048 - 44, 331,
     "16d320a6c7a48f99d74abb62ec92cc668f080220db1b7b4b708d0088f5e3bf07"),
)

def test_only_three_user_confirmed_samples():
    for name, size, hz, offset, fade, sha in CASES:
        assert name in S
        assert f"{size}u" in S and f"{hz}u" in S
        assert f"{fade}u" in S
        assert sha in S
        assert len(sha) == 64
        assert offset % 2 == 0
        assert offset < size
        assert 0 < fade < offset // 2
    assert "#define TAIL_COUNT (sizeof(tails)/sizeof(tails[0]))" in S

def test_optin_and_constraints():
    assert '"ExperimentalAudioCombatTailRepair"' in I
    assert 'GetPrivateProfileIntA("Debug","ExperimentalAudioCombatTailRepair",0,path)==1' in N
    assert "audio_combat_tail_repair.c" in B
    assert "SpiderCombatTailRepair_Init" in H
    assert "SpiderCombatTailRepair_Probe" in H
    assert "if(!armed || !buffer || looping || slot<0 || slot>=32)return;" in S
    assert "IDirectSoundBuffer_GetFormat" in S
    assert "fmt.nChannels!=1u" in S
    assert "fmt.wBitsPerSample!=16u" in S
    assert "fmt.nBlockAlign!=2u" in S
    assert "caps.dwBufferBytes!=bytes" in S
    assert "strcmp(actual,target->sha256)==0" in S
    assert "CryptHashData" in S
    assert "IDirectSoundBuffer_Unlock" in S
    for forbidden in (
        "IDirectSoundBuffer_Play(",
        "IDirectSoundBuffer_Stop(",
        "IDirectSoundBuffer_SetVolume(",
        "IDirectSoundBuffer_SetPan(",
        "IDirectSoundBuffer_SetFrequency(",
        "IDirectSoundBuffer_Restore(",
        "CreateFileA(",
        "WriteFile(",
    ):
        assert forbidden not in S

def test_capture_precedes_only_guarded_mutation():
    assert N.index("SpiderCombatPCM_Probe(slot,info.buffer,info.bytes") < N.index(
        "SpiderCombatTailRepair_Probe(slot,info.buffer,info.bytes")
    assert N.index("SpiderCombatTailRepair_Probe(slot,info.buffer,info.bytes") < N.index(
        "SpiderSwing5Fingerprint_Probe(slot,info.buffer")
    assert S.index("strcmp(actual,target->sha256)==0") < S.index(
        "write_byte(a,part1_bytes,b,i,0u)"
    )

def test_fade_preserves_prefix_and_zeroes_tail_offline():
    # Mirrors the integer-only C formula; regression expectations for
    # byte/samples at boundaries, without shipping known user data.
    for name, size, hz, offset, fade, sha in CASES:
        n = size // 2
        seam = offset // 2
        original = [(i * 321 + 11) % 31000 - 15500 for i in range(n)]
        after = original.copy()
        for i in range(seam - fade, seam):
            after[i] = int(original[i] * (seam - i) / (fade + 1))
        for i in range(seam, n):
            after[i] = 0
        assert after[:seam-fade] == original[:seam-fade]
        assert after[seam:] == [0] * (n-seam)
        assert abs(after[seam-1]) <= max(1, abs(original[seam-1]) // fade)
        assert 14.9 <= 1000 * fade / hz <= 15.1
