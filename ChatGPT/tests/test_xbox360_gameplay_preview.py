"""Manual Xbox 360 gameplay candidate: authenticated, reversible and opt-in.

Only A=0/B=1 and the player's device identity are currently user-measured.
All other digital buttons are test candidates (not XInput trigger claims).
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[2]
MAP = (ROOT / "xbox360_gameplay_mapping.h").read_text(encoding="utf-8")
CFG = (ROOT / "virtual_spidey_cfg.c").read_text(encoding="utf-8")
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
UI = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
API = (ROOT / "gamepad_ui_profile.h").read_text(encoding="utf-8")
VALIDATE = (ROOT / "ini_validation.h").read_text(encoding="utf-8")


def test_xbox_gameplay_mapping_reversible_and_reserves_start_back(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src = tmp_path / "xbox_mapping.c"
    exe = tmp_path / "xbox_mapping"
    src.write_text(textwrap.dedent(r"""
      #include <stdint.h>
      #include <string.h>
      #include "xbox360_gameplay_mapping.h"
      int main(void) {
        uint8_t original[SPIDER_PS1_CFG_SIZE],runtime[SPIDER_PS1_CFG_SIZE];
        uint32_t saved[SPIDER_PS1_BUTTON_COUNT];
        size_t i;
        (void)spider_ps1_apply; /* shared header PS1 function intentionally unused in Xbox-only path */
        for(i=0;i<sizeof(original);i++)original[i]=(uint8_t)(i*17u+5u);
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          spider_ps1_write_word(original,spider_ps1_offsets[i],(uint32_t)(12u+i));
        memcpy(runtime,original,sizeof(runtime));
        if(!spider_ps1_snapshot(runtime,sizeof(runtime),saved))return 1;
        if(!spider_xbox360_apply(runtime,sizeof(runtime)))return 2;
        if(spider_ps1_read_word(runtime,0x50)!=0u)return 3; /* A=Jump */
        if(spider_ps1_read_word(runtime,0x54)!=1u)return 4; /* B=Kick */
        if(!spider_xbox360_button_free_for_pause(6u) ||
           !spider_xbox360_button_free_for_pause(7u))return 5;
        if(spider_xbox360_button_free_for_pause(0u) ||
           spider_xbox360_button_free_for_pause(9u))return 6;
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          if(spider_ps1_read_word(runtime,spider_ps1_offsets[i])!=
             spider_xbox360_buttons[i])return 7;
        if(!spider_ps1_restore(runtime,sizeof(runtime),saved))return 8;
        if(memcmp(original,runtime,sizeof(runtime)))return 9;
        if(spider_xbox360_apply(runtime,sizeof(runtime)-1u))return 10;
        if(memcmp(original,runtime,sizeof(runtime)))return 11;
        return 0;
      }
    """), encoding="utf-8")
    subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_xbox_preview_only_after_verified_early_device_and_native_readonly():
    assert '"ControllerXbox360Preview"' in VALIDATE
    assert 'ControllerXbox360Preview=0' in CFG
    assert 'GetPrivateProfileIntA("Input","ControllerXbox360Preview",0,ini)==1' in UI
    assert 'SpiderGamepadUI_PrepareXbox360(early)' in CORE
    assert 'SpiderCfg_EnableXbox360Preview();' in CORE
    assert 'SpiderCfg_BootstrapGameRead();' in CORE
    assert 'SpiderCfg_Xbox360BootstrapLoaded()' in CORE
    assert 'SpiderGamepadUI_ActivateXbox360();' in CORE
    assert 'SpiderCfg_CancelXbox360Preview();' in CORE
    assert CORE.index('SpiderCfg_EnableXbox360Preview();') < CORE.index('SpiderCfg_BootstrapGameRead();')
    assert CORE.index('SpiderCfg_BootstrapGameRead();') < CORE.index('SpiderGamepadUI_ActivateXbox360();')
    assert 'int SpiderGamepadUI_PrepareXbox360(HWND hwnd)' in UI
    assert 'detected_kind!=SPIDER_RECONNECT_XBOX360' in UI
    assert 'profile==SPIDER_PROFILE_DUALSHOCK4' in UI
    assert 'GetForegroundWindow()!=hwnd' in UI
    assert 'xbox_try_readonly_menu();' in UI
    assert 'if(!xbox_readonly_active)' in UI
    assert 'else xbox_readonly_active=1;' in UI
    assert 'int SpiderGamepadUI_PrepareXbox360(HWND game_window);' in API
    assert 'void SpiderGamepadUI_ActivateXbox360(void);' in API


def test_ps1_and_xbox_overlays_are_mutually_exclusive_and_saved_bytes_restored():
    assert 'if(xbox360_preview_active) {LeaveCriticalSection(&guard);return 0;}' in CFG
    assert 'if(ps1_preview_active || !spider_xbox360_apply(cfg,CFG_SIZE))' in CFG
    assert 'if(ps1_preview_active || xbox360_preview_active)' in CFG
    assert 'spider_ps1_restore(saved_cfg,CFG_SIZE,original_joystick)' in CFG
    assert 'spider_ps1_snapshot(cfg,CFG_SIZE,original_joystick)' in CFG
    assert 'DWORD code=(ps1_preview_active || xbox360_preview_active)?' in CFG
    assert 'if(ps1_preview_active)spider_ps1_apply(cfg,CFG_SIZE);' in CFG
    assert 'else spider_xbox360_apply(cfg,CFG_SIZE);' in CFG
    assert 'if((ps1_preview_active || xbox360_preview_active) && opens>opened && reads>served)' in CFG
    assert 'xbox360_preview_active=0;' in CFG
    assert 'XBOX360-PREVIEW ROLLBACK' in CFG


def test_start_back_pause_uses_actual_live_map_and_controller_paused_ui():
    guard = UI.split("static int xbox_pause_button_free_of_native_actions(", 1)[1].split(
        "static BOOL CALLBACK enumerate_gamepad(", 1)[0]
    assert 'if(xbox360_preview_active || SpiderNativeReconnect_CanonicalXboxMapping())' in guard
    assert 'return spider_xbox360_button_free_for_pause(requested_button);' in guard
    assert 'GetPrivateProfileStringA("Joystick",native_actions[i],fallback,' in guard
    assert 'if(!parse_native_button(value,&button) || button==requested_button)' in guard
    assert 'native_pause_active;' in UI
    assert 'pause_requested=(fresh & (1u<<SPIDER_XBOX360_BACK_DI_BUTTON))!=0' in UI
    assert 'GAMEPAD-XBOX360: 3D MENU Back -> Escape' in UI
    assert 'SPIDER_PROFILE_XBOX360' in UI
    assert 'spider_ps1_native_button_label' not in guard


def test_ps1_equivalent_xbox_face_and_shoulder_layout_is_explicit():
    # The physical labels for buttons 2..5 are standardized XUSB *candidates*
    # pending the user's actual 045E:028E Holtek Wine verification.
    assert "0u,1u,2u,3u,4u,5u,9u" in MAP
    for intended in (
        "Xbox A  -> Jump", "Xbox B  -> Kick", "Xbox X  -> Punch",
        "Xbox Y  -> Web", "Xbox LB -> Aim", "Xbox RB -> Zipline",
        "Xbox RT -> Swing",
    ):
        assert intended in MAP
    assert "TARGET ONLY, NOT IMPLEMENTED HERE" in MAP
    assert "TEMPORARY DIGITAL SWING FALLBACK" in MAP
    assert "BUTTON9=Swing" not in MAP  # not a claim that BUTTON9 is RT


def test_xbox_trigger_diagnostic_is_optin_passive_bounded_and_foreground_only():
    assert '"Xbox360TriggerProbe"' in VALIDATE
    assert 'Xbox360TriggerProbe=0' in CFG
    assert 'GetPrivateProfileIntA("Debug","Xbox360TriggerProbe",0,ini)==1' in UI
    assert "static void xbox_trace_trigger_axes(const DIJOYSTATE2 *now)" in UI
    assert "if(!now || xbox_trigger_probe_samples>=48u)return;" in UI
    assert "(DWORD)(tick-xbox_trigger_probe_tick)<100u" in UI
    assert "if(xbox_trigger_probe_requested && detected_kind==SPIDER_RECONNECT_XBOX360 &&" in UI
    assert "gameplay_3d && !bink_active && !native_pause_active)" in UI
    assert 'xbox_trace_trigger_axes(&state);' in UI
    block=UI.split("static void xbox_trace_trigger_axes(",1)[1].split(
        "void SpiderGamepadUI_OnFlip(",1)[0]
    for field in ("lZ","lRx","lRy","lRz","rglSlider[0]","rglSlider[1]"):
        assert field in block
    assert "GAMEPAD-XBOX360: TRIGGER-PROBE" in block
    for forbidden in ("SendInput", "VirtualProtect", "WritePrivateProfileStringA",
                      "spider_xbox360_apply", "begin_scancode", "GetDeviceState"):
        assert forbidden not in block
