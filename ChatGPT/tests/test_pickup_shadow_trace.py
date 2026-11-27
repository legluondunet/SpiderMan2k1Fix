"""Read-only authenticated native pickup/Shadow2 world observation."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_shadow_trace_is_bounded_and_passive():
    h=(ROOT/"pickup_shadow_trace.h").read_text()
    c=(ROOT/"spiderman2k1fix.c").read_text()
    assert "#define SPIDER_SHADOW_NODES 128u" in h
    assert "#define SPIDER_SHADOW_PICKUPS 16u" in h
    assert "#define SPIDER_SHADOW_PERIODIC_MAX 120u" in h
    assert "#define SPIDER_SHADOW_F9_MAX 12u" in h
    assert "ReadProcessMemory(GetCurrentProcess()" in h
    assert "received==length" in h
    assert "shadow_trace_u32(owner)!=SPIDER_SHADOW_PICKUP_VTABLE" in h
    assert "shadow_trace_u32(quad)==SPIDER_SHADOW_QUAD_VTABLE" in h
    assert "0x10,0x3c,0x48,0x54" in h
    assert "center-minus-ground-x4" in h
    assert "if(duplicate)" in h
    assert "shadow_trace_thread!=GetCurrentThreadId()" in h
    for s in ("WriteProcessMemory(", "VirtualProtect(", "VirtualAlloc(", "malloc(", "SendInput("):
        assert s not in h
    for address in ("0x46B00Eu","0x46B352u","0x409144u","0x460672u","0x409005u","0x461132u"):
        assert "shadow_trace_signature("+address in h
    assert 'GetPrivateProfileIntA("Debug","ShadowWorldTrace",0,escape_ini)==1' in c
    assert "shadow_trace_snapshot(count,0)" in c
    assert "if(queued && SUCCEEDED(unlock_hr) && !f9_static_comic_capture" in c
    assert "shadow_trace_snapshot(InterlockedCompareExchange(&flip_count,0,0),1)" in c
    assert "SpiderQuickExitF12_InstallEscapeReaderTrace();" in c
    assert "ShadowWorldTrace=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
