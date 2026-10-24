"""Validated HUD correction starts on unless explicitly disabled.

The supported game EXE is verified before any native hook; original 4:3
and an explicit HUDWidescreenFix=0 remain safe/no-transform configurations.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
GENERATOR = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
INIS = (
    "SpiderMan2k1Fix.ini",
    "SpiderMan2k1Fix.generated-example.ini",
    "examples/SpiderMan2k1Fix.user-clean.ini",
)


def test_missing_option_activates_hud_and_zero_is_explicit_opt_out():
    # A missing section/key yields 1, but existing HUDWidescreenFix=0 is
    # honored exactly (the game must not rewrite the user's existing INI).
    assert 'option=GetPrivateProfileIntA("HUD","HUDWidescreenFix",1,ini);' in CORE
    assert 'if (option==0) {' in CORE
    assert 'if (option!=1) {' not in CORE
    assert 'GetPrivateProfileIntA("HUD","HUDWidescreenFix",0,ini)' not in CORE
    assert "SpiderHudWidescreen_Install(exe_verified,TRUE)" in CORE


def test_original_four_by_three_never_installs_widescreen_transforms():
    assert 'lstrcmpiA(display_aspect_setting,"4:3")==0 && option!=0' in CORE
    assert 'log_line("HUD: disabled automatically for original 4:3 mode")' in CORE
    assert 'SpiderHudWidescreen_InstallScopesOnly(exe_verified)' in CORE
    assert "if (!SpiderHudWidescreen_Install(exe_verified,TRUE))" in CORE
    assert "supported_exe()" in CORE


def test_every_factory_ini_enables_hud():
    for path in INIS:
        contents = (ROOT / path).read_text(encoding="utf-8")
        assert contents.count("HUDWidescreenFix=1") == 1, path
        assert "HUDWidescreenFix=0" not in contents, path
        assert "0 explicitly disables it" in contents, path


def test_created_ini_and_existing_user_ini_behave_differently():
    assert '"HUDWidescreenFix=1\\r\\n"' in GENERATOR
    assert '"HUDWidescreenFix=0\\r\\n"' not in GENERATOR
    assert "SpiderCfg_EnsureDefaultIni" in GENERATOR
    # Existing-user values pass validation/repair and remain valid opt-outs.
    validator = (ROOT / "ini_validation.h").read_text(encoding="utf-8")
    assert '"HUDWidescreenFix","NativeESC4x3","NativeMotif4x3","NativeCursor4x3"' in validator
    assert "option==0" in CORE


def test_logging_no_longer_calls_feature_unvalidated():
    assert "HUD: adaptive widescreen correction enabled" in CORE
    assert "HUD: EXPERIMENTAL native 16-site widescreen correction installed" not in CORE
    assert "HUD-ADAPTIVE: strict geometry correction enabled" in CORE


def test_obsolete_f12_hud_sampling_removed_without_touching_validated_hooks():
    # Former F12 six-frame HUD probe is no longer needed at runtime.
    for obsolete in (
        "SPIDERMAN2K1FIX_HUD_TRACE", "HUD-PROBE:",
        "HUD-PROBE-EXTRA:", "HUD-PROBE-BLT:",
        "hud_probe_enabled", "hud_probe_flip(", "hud_probe_draw(",
        "hud_probe_boss_unknown(", "hud_probe_boss_blt",
    ):
        assert obsolete not in CORE

    for validated in (
        "SpiderHudWidescreen_Install(exe_verified,TRUE)",
        "maybe_correct_adaptive_hud(", "maybe_correct_native_pause43(",
        "maybe_correct_native_motif43(", "maybe_correct_native_cursor43(",
        "SpiderDI8Retry_OnFlip(count);",
        "SpiderInputFocus_OnFlip(count);", "pause_at_flip_if_background();",
        "SpiderQuickExitF12_ObserveEngineState(",
        "install_f9_png_only_dispatch();",
        "static int geom_rva(void *ret,DWORD *rva) {",
    ):
        assert validated in CORE

    draw=CORE.split("static HRESULT WINAPI hooked_draw_primitive(",1)[1].split(
        "static HRESULT WINAPI hooked_draw_indexed_primitive(",1)[0]
    assert draw.index("maybe_correct_adaptive_hud(") < draw.index(
        "maybe_correct_native_motif43(")
    assert draw.index("maybe_correct_native_motif43(") < draw.index(
        "maybe_correct_native_cursor43(")
    assert draw.index("maybe_correct_native_cursor43(") < draw.index(
        "maybe_correct_native_pause43(")
    assert "original_draw_primitive(self,type,fvf,(LPVOID)v,count,flags)" in draw


def test_hud_probe_cleanup_does_not_remove_runtime_f12_or_f9_game_features():
    assert "SpiderQuickExitF12_AttachWindow(hwnd);" in CORE
    assert "SpiderQuickExitF12_Init(log_line);" in CORE
    assert "SpiderQuickExitF12_ObserveComicRender(" in CORE
    assert "SpiderQuickExitF12_ObserveMovieFrame();" in CORE
    assert "SpiderQuickExitF12_ObserveEngineState(" in CORE
    assert "SpiderDI8Retry_OnFlip(count);" in CORE
    assert "install_f9_png_only_dispatch();" in CORE
