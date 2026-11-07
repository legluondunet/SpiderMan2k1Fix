"""No inherited gamepad presets: first-use Xbox360 follows SDL3 alone.

The removed fixed Xbox/DS4 preview and native DirectInput mapping tests
are replaced by assertions about the actual supported SDL3 source,
read-only original-game configuration, and a host-compiled generic
per-model default. The retail game's own Joystick defaults are not a
SpiderMan2k1Fix preset and are intentionally preserved.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
RETIRED=(
    "ps1_gamepad_mapping.h",
    "ps1_native_start_mapping.h",
    "xbox360_gameplay_mapping.h",
    "xbox360_menu_policy.h",
    "xbox360_trigger_swing_policy.h",
    "sdl3_gameplay_native_map.h",
)


def test_old_hardcoded_profile_files_are_deleted():
    assert not any((ROOT/p).exists() for p in RETIRED)


def test_no_pre_sdl3_custom_preset_in_original_game_config():
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    cfg=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    probe=(ROOT/"native_joystick_swap_probe.h").read_text(encoding="utf-8")
    for src in (core,cfg,native,ui,live,probe):
        for token in (
            "spider_ps1_buttons", "spider_xbox360_buttons",
            "spider_ps1_apply", "spider_xbox360_apply",
            "spider_native_swap_preset",
            "spider_xbox360_menu_edges",
            "spider_sdl3_native_xbox360_seven",
        ):
            assert token not in src
    assert "SpiderCfg_BootstrapGameRead();" in core
    assert "SpiderCfg_InstallModernKeyboardDefaults();" in core
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in core
    assert "SpiderSDL3_InstallDirectJoystickEntry" in ui
    assert "SpiderPS1Mouse_OnFlip(hwnd);" in ui
    assert 'try_rebind(' not in native
    assert "SpiderNativeReconnect_CanonicalXboxMapping(void) {return 0;}" in native
    assert "SpiderNativeReconnect_CanonicalDS4Mapping(void) {return 0;}" in native
    assert "spider_native_swap_read(" in probe
    assert "spider_ps1_buttons" not in probe
    assert "SpiderCfg_EnableXbox360Preview(void) {return 0;}" in cfg
    assert "SpiderCfg_EnablePS1Preview(void) {return 0;}" in cfg
    assert "SpiderSDL3GameplayLive_VerifiedXboxSwingMap(void) {return 0;}" in live


def test_clean_user_ini_does_not_generate_old_xbox_preview():
    src=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    assert '"ExperimentalSDL3UniversalGameplay=1' not in src
    assert "requested=1; /* SDL3-only gameplay, no legacy-INI opt-out. */" in (ROOT/"sdl3_universal_gameplay.c").read_text()
    assert '"ControllerXbox360Preview=0' not in src
    assert '"ControllerPS1Preview=0' not in src
    assert '"ControllerXbox360RTSwing=0' not in src
    assert '"ExperimentalNativeJoystickMappingSwap=' not in src
    assert '"ControllerProfile=Auto' not in src


def test_first_time_xbox_controller_gets_generic_sdl3_defaults(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"first_use.c"
    exe=tmp_path/"first_use"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_controller_profile.h"
        int main(void) {
          SpiderSDL3ProfileBank bank;
          SpiderSDL3ControllerProfile *xbox,*ps3;
          spider_sdl3_bank_reset(&bank);
          xbox=spider_sdl3_bank_get(&bank,0x045eu,0x028eu);
          assert(xbox && spider_sdl3_profile_valid(xbox));
          assert(xbox->actions[SPIDER_SDL3_ACTION_JUMP].kind==SPIDER_SDL3_BIND_BUTTON);
          assert(xbox->actions[SPIDER_SDL3_ACTION_JUMP].index==0u);
          assert(xbox->actions[SPIDER_SDL3_ACTION_SWING].kind==SPIDER_SDL3_BIND_TRIGGER);
          assert(xbox->actions[SPIDER_SDL3_ACTION_SWING].index==SPIDER_SDL3_AXIS_RT);
          assert(xbox->actions[SPIDER_SDL3_ACTION_PAUSE].index==4u);
          ps3=spider_sdl3_bank_get(&bank,0x054cu,0x0268u);
          assert(ps3 && spider_sdl3_profile_valid(ps3));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_first_use_does_not_forge_unknown_device():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "SDL_GetGamepads" in probe
    assert "SDL_IsGamepad" in probe
    assert "SDL_AddGamepadMappingsFromFile" in probe
    assert "SpiderSDL3Probe_SelectedMappedSample(" in backend
    assert 'restore_original("no selected mapped controller");' in backend
    assert "spider_sdl3_profile_default(" in (
        ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
