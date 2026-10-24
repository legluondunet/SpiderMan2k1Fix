"""Win11 native Alt+Tab passive focus tracing regression guards."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
S=(ROOT/"input_focus_trace.c").read_text()
DOC=(ROOT/"ChatGPT/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_passive_trace_does_not_install_directinput_hooks():
    anchor=S.index('GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_PASSIVE"')
    end=S.index('if(!want_trace && !want_fix)',anchor)
    branch=S[anchor:end]
    assert "InterlockedExchange(&diagnostic_active,1)" in branch
    assert "InterlockedExchange(&reacquire_enabled,0)" in branch
    assert "DirectInput8 hook NOT installed" in branch
    assert "return;" in branch
    assert "original_directinput8_create=(DICreate8Fn)*slot" not in branch

def test_game_window_focus_and_geometry_diagnostics():
    for msg in ("WM_ACTIVATEAPP", "WM_KILLFOCUS", "WM_SETFOCUS",
                "WM_SIZE", "WM_DISPLAYCHANGE", "WM_WINDOWPOSCHANGED"):
        assert msg in S
    assert "window-geometry event=%s" in S
    assert "GetWindowRect(hwnd,&window);" in S

def test_launch_does_not_enable_invasive_input_diagnostics():
    assert "SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'" in DOC
    assert "SPIDERMAN2K1FIX_D3D_INPLACE='1'" in DOC
    assert "ExperimentalAltTabReacquire=0" in DOC
    assert "ExperimentalFocusTrace=0" in DOC
