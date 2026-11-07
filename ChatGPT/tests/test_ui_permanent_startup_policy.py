"""Native Pause composition, veil and scene iris are guarded built-in features."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
INI=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")


def test_permanent_ui_activation_is_after_hud_install_and_widescreen_only():
    start=CORE.index("  install_player_hud(TRUE);")
    finish=CORE.index("  native_pause43_init();",start)
    init=CORE[start:finish]
    assert "if(SpiderHudWidescreen_Installed() &&" in init
    assert 'lstrcmpiA(display_aspect_setting,"4:3")!=0' in init
    assert "native_esc43_supported_monitor()" not in init
    assert "desktop_w/desktop_h are still ZERO here" in init
    # Monitor dimensions are captured later by DirectDraw, never before.
    assert "static DWORD desktop_w=0, desktop_h=0;" in CORE
    assert CORE.index("static void probe_desktop_mode(HWND hwnd)") < CORE.index("  install_player_hud(TRUE);")
    assert "probe_desktop_mode(hwnd);" in CORE
    assert "native_esc43_supported_monitor() &&" in CORE
    assert init.index("install_player_hud(TRUE);") < init.index("SpiderHudWidescreen_SetUICanvasPrototype(TRUE);")
    for flag in ("ui_canvas_trial_enabled", "ui_transition_bounds25_enabled",
                 "ui_compositor_trial_enabled", "ui_compositor_native_backdrop_enabled"):
        assert f"InterlockedExchange(&{flag},1);" in init
    assert "InterlockedExchange(&ui_compositor_hud_over_veil_enabled,0);" in init
    assert "original rendering preserved" in init


def test_legacy_ui_environment_switches_are_not_read():
    for key in ("SPIDERMAN2K1FIX_UI_CANVAS_TEST",
                "SPIDERMAN2K1FIX_UI_COMPOSITOR_TEST",
                "SPIDERMAN2K1FIX_UI_COMPOSITOR_NATIVE_BACKDROP",
                "SPIDERMAN2K1FIX_UI_COMPOSITOR_HUD_OVER_VEIL",
                "SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS_TEST",
                "SPIDERMAN2K1FIX_NATIVE_ESC_43",
                "SPIDERMAN2K1FIX_NATIVE_MOTIF_43",
                "SPIDERMAN2K1FIX_NATIVE_CURSOR_43"):
        assert key not in CORE


def test_pause_and_scene_are_source_verified_not_generic_draw_overlays():
    assert "static int native_esc43_hud_setting_enabled(void)" in CORE
    assert "SpiderHudWidescreen_Installed()!=0;" in CORE
    assert "ui_comp_draw_native_exterior_gutters(" in CORE
    assert "SpiderNativeBounds25_Observe(" in CORE
    assert "SpiderNativeBounds25_Widths(" in CORE
    assert "IDirect3DDevice7_ApplyStateBlock(device,saved)" in CORE
    assert '"[HUD]\\r\\n"' not in INI
    for name in ("NativeESC4x3", "NativeMotif4x3", "NativeCursor4x3"):
        assert f'"{name}=' not in INI
