"""Read-only, opt-in DirectDraw7 diagnosis of native F9 screenshots.

These tests enforce minimal risk to the validated HUD/borderless/pause path.
Windows/Wine in-game confirmation is still required: passing tests do not
prove that the legacy game's screenshot bug has been fixed.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")


def test_trace_is_disabled_by_default_and_must_be_requested_explicitly():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_F9_TRACE"' in SRC
    assert "if(debug_log_enabled &&" in SRC
    assert "InterlockedExchange(&f9_trace_enabled,1);" in SRC
    assert "static volatile LONG f9_trace_enabled=0" in SRC
    assert "InterlockedCompareExchange(&f9_trace_enabled,0,0)==1" in SRC


def test_surface_vtable_indices_and_callbacks():
    for slot, symbol in (
        (17, "hooked_f9_surface_get_dc"),
        (22, "hooked_f9_surface_get_desc"),
        (25, "hooked_f9_surface_lock"),
        (26, "hooked_f9_surface_release_dc"),
        (32, "hooked_f9_surface_unlock"),
    ):
        assert f"patched_surface_vtable[{slot}]=(void *){symbol};" in SRC
    # No changes to the longstanding Blt, Flip and attached-surface hooks.
    for slot, symbol in ((5, "hooked_blt"), (11, "hooked_flip"),
                         (12, "hooked_get_attached")):
        assert f"patched_surface_vtable[{slot}]=(void *){symbol};" in SRC
    assert "if(InterlockedCompareExchange(&f9_trace_enabled,0,0)==1) {" in SRC


def test_f9_trace_passes_through_directdraw_calls():
    signatures = {
        "get_dc": "original_get_dc(self,dc)",
        "get_desc": "original_surface_get_desc(self,desc)",
        "lock": "original_surface_lock(self,rect,desc,flags,event)",
        "release_dc": "original_release_dc(self,dc)",
        "unlock": "original_surface_unlock(self,rect)",
    }
    for op, delegated_call in signatures.items():
        name = {
            "get_dc": "get_dc",
            "get_desc": "get_desc",
            "lock": "lock",
            "release_dc": "release_dc",
            "unlock": "unlock",
        }[op]
        marker = f"static HRESULT WINAPI hooked_f9_surface_{name}("
        assert marker in SRC
        piece = SRC.split(marker, 1)[1].split("\n}", 1)[0]
        assert delegated_call in piece
        assert "return hr;" in piece
        assert "VirtualProtect" not in piece
        assert "DDBLT_COLORFILL" not in piece


def test_reports_callsite_and_readback_metadata_only():
    for label in ("F9-TRACE: GetDC owner=",
                  "F9-TRACE: GetSurfaceDesc owner=",
                  "F9-TRACE: Lock owner=",
                  "F9-TRACE: ReleaseDC owner=",
                  "F9-TRACE: Unlock owner="):
        assert label in SRC
    assert "geom_rva(__builtin_return_address(0),&rva);" in SRC
    assert "f9_surface_kind(self)" in SRC
    assert 'return "PRIMARY";' in SRC
    assert 'return "3D-BACKBUFFER";' in SRC
    assert 'return "OTHER";' in SRC
    assert "GetAsyncKeyState(VK_F9)&0x8000" in SRC
    assert "InterlockedIncrement(&f9_trace_logged)<=200" in SRC
    assert "return event<=24;" in SRC


def test_f9_trace_keeps_gameplay_fix_and_pause_unchanged():
    for part in (
        "maybe_correct_adaptive_hud(",
        "SpiderHudWidescreen_Install(exe_verified,TRUE)",
        "pause_at_flip_if_background();",
        "SpiderInputFocus_OnFlip(count);",
        "SpiderDI8Retry_OnFlip(count);",
        "hr=original_blt(self,presented_dest,source,source_rect,flags,fx);",
        "original_blt(self,&rc,borderless_back,NULL,DDBLT_WAIT,NULL)",
        "if (!supported_exe()) {",
    ):
        assert part in SRC
    assert "F9-TRACE: opt-in native screenshot readback observation armed" in SRC
    assert "F9-TRACE: F9 key pressed; readback trace window OPEN" in SRC
