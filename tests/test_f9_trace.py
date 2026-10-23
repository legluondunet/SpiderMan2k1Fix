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
    assert "if(debug_log_enabled) {" in SRC
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
    assert "if(InterlockedCompareExchange(&f9_trace_enabled,0,0)==1 ||" in SRC
    assert "InterlockedCompareExchange(&f9_backbuffer_fix_enabled,0,0)==1) {" in SRC


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

def test_historical_backbuffer_readback_path_remains_inactive():
    # 0x1034D4/0x103537 were measured from a real Win11 F9 log:
    # the legacy game reads PRIMARY and creates all-magenta BMPs.
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_F9_BACKBUFFER"' not in SRC
    assert 'static volatile LONG f9_backbuffer_fix_enabled=0;' in SRC
    assert 'rva==0x001034D4u' in SRC
    assert 'rva==0x00103537u' in SRC
    assert 'self==(void *)borderless_primary && borderless_back' in SRC
    assert '(flags&DDLOCK_READONLY)!=0' in SRC
    assert 'rect==NULL && desc && desc->dwSize==sizeof(*desc)' in SRC
    assert 'original_surface_lock(back,rect,desc,flags,event)' in SRC
    assert 'original_surface_unlock(back,rect)' in SRC
    assert 'IDirectDrawSurface7_AddRef(back);' in SRC
    assert 'IDirectDrawSurface7_Release(back);' in SRC
    assert 'f9_backbuffer_lock_thread==GetCurrentThreadId()' in SRC
    assert 'f9_backbuffer_locked_surface=back;' in SRC
    assert 'f9_backbuffer_locked_surface=NULL;' in SRC
    assert 'InterlockedExchange(&f9_backbuffer_lock_active,0);' in SRC


def test_failed_redirect_falls_back_to_unmodified_primary():
    lock = SRC.split('static HRESULT WINAPI hooked_f9_surface_lock(',1)[1].split(
        'static HRESULT WINAPI hooked_f9_surface_release_dc(',1
    )[0]
    assert 'if(SUCCEEDED(back_hr))' in lock
    assert 'original_surface_unlock(back,rect);' in lock
    assert '*desc=requested;' in lock
    assert 'falling back to untouched PRIMARY screenshot' in lock
    assert 'hr=original_surface_lock(self,rect,desc,flags,event);' in lock
    assert 'desc->ddpfPixelFormat.dwRGBBitCount==32' in lock
    assert 'desc->dwWidth==desktop_w && desc->dwHeight==desktop_h' in lock
    assert 'desc->lPitch>=(LONG)(desktop_w*4u)' in lock
    assert 'desc->lpSurface!=NULL' in lock
    assert 'DDLOCK_WRITEONLY' not in lock


def test_png_default_does_not_enable_bmp_readback_or_change_alt_tab():
    assert "static volatile LONG f9_backbuffer_fix_enabled=0" in SRC
    assert 'if(debug_log_enabled &&' in SRC
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_F9_BACKBUFFER"' not in SRC
    assert 'InterlockedExchange(&f9_backbuffer_fix_enabled,1);' not in SRC
    assert "hr=original_surface_unlock(self,rect);" in SRC
    for f in ('maybe_correct_adaptive_hud(', 'pause_at_flip_if_background();',
              'SpiderInputFocus_OnFlip(count);', 'SpiderDI8Retry_OnFlip(count);',
              'SpiderHudWidescreen_Install(exe_verified,TRUE)'):
        assert f in SRC
