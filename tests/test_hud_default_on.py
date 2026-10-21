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
    assert 'spider_ini_hud[]={"HUDWidescreenFix"}' in validator
    assert "option==0" in CORE


def test_logging_no_longer_calls_feature_unvalidated():
    assert "HUD: adaptive widescreen correction enabled" in CORE
    assert "HUD: EXPERIMENTAL native 16-site widescreen correction installed" not in CORE
    assert "HUD-ADAPTIVE: strict geometry correction enabled" in CORE
