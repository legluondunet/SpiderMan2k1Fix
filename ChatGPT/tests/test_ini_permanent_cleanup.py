"""Permanent compatibility behaviors and minimal first-run INI contract.

These tests verify source wiring; runtime behavior still requires Windows/Wine.
"""
from pathlib import Path
import configparser

ROOT=Path(__file__).resolve().parents[2]
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
EXAMPLE=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
CORE=(ROOT/"spiderman2k1fix.c").read_text()
RETRY=(ROOT/"input_notacquired_retry.c").read_text()
BINK=(ROOT/"focus_media_pause.c").read_text()
DS=(ROOT/"focus_dsound_pause.c").read_text()
PAUSE=(ROOT/"pause_surface.c").read_text()
FOCUS=(ROOT/"input_focus_trace.c").read_text()
VALIDATOR=(ROOT/"ini_validation.h").read_text()

def test_player_only_ini_keeps_fps_as_real_control():
    cfg=configparser.ConfigParser(interpolation=None)
    cfg.read_string(EXAMPLE)
    assert set(cfg.sections())=={"Debug","Display","Input","Audio","Keyboard","Joystick","Internal"}
    assert list(cfg["Display"])==["aspectratio","brightness","fpslimit"]
    assert cfg["Display"]["FPSLimit"]=="30"
    assert cfg["Input"]["KeyboardLayout"]=="Auto"
    assert list(cfg["Input"])==["keyboardlayout"]
    assert cfg["Debug"]["Log"]=="0"
    assert len(cfg["Internal"]["RawHex"])==504
    assert '"FPSLimit=30\\r\\n"' in GEN
    assert 'GetPrivateProfileStringA("Display","FPSLimit","",fps_value' in CORE
    assert "number>=0 && number<=240" in CORE
    assert "SpiderFrameLimiter_Init(&g_frame_limiter,(unsigned)fps)" in CORE
    assert '"Resolution=Auto\\r\\n"' not in GEN
    assert '"HUDWidescreenFix=1\\r\\n"' not in GEN

def test_focus_system_initialized_permanently_and_guarded():
    assert "InterlockedExchange(&pause_on_focus_loss,1);" in CORE
    assert "SpiderFocusMedia_Pause();" in CORE
    assert "SpiderFocusDSound_Pause();" in CORE
    assert "SpiderPauseAny_Begin(hwnd)" in CORE
    assert "SpiderPauseSurface_Begin(" in CORE
    assert "InterlockedExchange(&requested,1);" in RETRY
    assert "if(flip_index==120) try_install();" in RETRY
    assert "memcmp(SPIDER_BINK_CALLSITE,bink_signature" in BINK
    assert "memcmp(SPIDER_VOICE_INIT,voice_init_signature" in DS
    assert 'GetPrivateProfileIntA("Input","PauseBinkOnFocusLoss"' not in BINK
    assert 'GetPrivateProfileIntA("Input","PauseDirectSoundOnFocusLoss"' not in DS
    assert 'GetPrivateProfileIntA("Input","RestoreCursorOnFocusLoss"' not in FOCUS
    assert "want_cursor=1;" in FOCUS
    assert "enabled_dim=1;" in PAUSE
    assert "enabled_grid=0;" in PAUSE
    assert "enabled_banner=1;" in PAUSE
    assert "opacity=50;" in PAUSE
    assert "DisableProcessWindowsGhosting" in CORE
    assert "want_interface_probe=0;" in FOCUS  # unsafe COM paths stay quarantined
    assert "want_fix=0;" in FOCUS

def test_permanent_graphics_and_legacy_ini_migration_safe():
    assert 'SpiderFOV_Init("Auto",1,0,log_line);' in CORE
    assert "SpiderDisplayMenu_Install(display_resolution_setting,display_aspect_setting,log_line);" in CORE
    assert "SpiderHudWidescreen_Install(exe_verified,TRUE)" in CORE
    assert 'GetPrivateProfileIntA("HUD","HUDWidescreenFix"' not in CORE
    assert "InterlockedExchange(&comic_native_aspect_enabled," in CORE
    assert "InterlockedExchange(&media_native_blt_anysize_enabled,0);" in CORE
    assert 'strcpy(configured_resolution,"Auto");' in GEN
    assert "deprecated_ini_count(ini)" in GEN
    assert "if(deprecated_ini_key(e->section,e->key))continue;" in GEN
    assert 'WritePrivateProfileStringA("Input","KeyboardLayout","Off",tmp)' in GEN
    assert 'CopyFileA(ini,backup,TRUE)' in GEN
    assert "MoveFileExA(tmp,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)" in GEN
    assert "SpiderIni_Validate(tmp,NULL,NULL)" in GEN
    assert "ini_writes_permitted=0;" in GEN
    assert '"PauseOnFocusLoss"' in VALIDATOR  # accepted only for migration
    assert "FPSLimit" not in GEN[GEN.index("static const struct {const char *section,*key;} deprecated_ini_keys[]"):GEN.index("static int deprecated_ini_key(", GEN.index("deprecated_ini_keys[]"))]
