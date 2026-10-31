"""Experimental DS4 live SDL3 3D mapping: portable translation and source safety.

Real Windows hardware is required to show the game actually consumes the
seven mapped DirectInput button indices in 3D. This test does not claim it.
"""
from pathlib import Path
import subprocess
import shutil
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_ds4_sdl_bindings_to_native_seven_c99(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"mapping.c";exe=tmp_path/"mapping"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "sdl3_gameplay_native_map.h"
        int main(void) {
          SpiderSDL3ControllerProfile ds4,xbox;
          uint32_t native[SPIDER_PS1_BUTTON_COUNT]={0};
          uint32_t snapshot[SPIDER_PS1_BUTTON_COUNT]={0};
          unsigned i;
          assert(spider_sdl3_profile_default(&ds4,0x054c,0x09cc));
          assert(spider_sdl3_profile_default(&xbox,0x045e,0x028e));
          assert(spider_sdl3_native_ds4_seven(&ds4,native));
          for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
            assert(native[i]==spider_ps1_buttons[i]);
          assert(!spider_sdl3_native_ds4_seven(&xbox,native));
          /* Reassign the physical Square button to Jump: native index 0
             becomes Jump and physical Cross becomes Punch. */
          assert(spider_sdl3_profile_assign(&ds4,SPIDER_SDL3_ACTION_JUMP,
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
          assert(spider_sdl3_native_ds4_seven(&ds4,native));
          assert(native[SPIDER_SDL3_ACTION_JUMP]==0);
          assert(native[SPIDER_SDL3_ACTION_PUNCH]==1);
          /* Sony L2 is deliberately refused: no independently
             verified DirectInput rgbButton index for that axis. */
          memcpy(snapshot,native,sizeof(snapshot));
          assert(spider_sdl3_profile_assign(&ds4,SPIDER_SDL3_ACTION_SWING,
            spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT)));
          assert(!spider_sdl3_native_ds4_seven(&ds4,native));
          assert(!memcmp(snapshot,native,sizeof(snapshot)));
          assert(spider_sdl3_profile_assign(&ds4,SPIDER_SDL3_ACTION_SWING,
            spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_RT)));
          assert(spider_sdl3_native_ds4_seven(&ds4,native));
          assert(native[SPIDER_SDL3_ACTION_SWING]==7);
          /* D-pad is POV, not a native rgbButton; refuse atomic map,
             preserve caller's output array unchanged on failure. */
          memcpy(snapshot,native,sizeof(snapshot));
          assert(spider_sdl3_profile_assign(&ds4,SPIDER_SDL3_ACTION_WEB,
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,11)));
          assert(!spider_sdl3_native_ds4_seven(&ds4,native));
          assert(!memcmp(snapshot,native,sizeof(snapshot)));
          assert(!spider_sdl3_ds4_native_button(
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,20),&native[0]));
          assert(!spider_sdl3_ds4_native_button(
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,7),&native[0]));
          assert(!spider_sdl3_ds4_native_button(
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6),&native[0]));
          assert(!spider_sdl3_ds4_native_button(
            spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,0),&native[0]));
          assert(!spider_sdl3_ds4_native_button(
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,0),0));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_live_native_owner_never_double_injects_seven_gameplay_actions():
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    plan=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    main=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    val=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert '"ExperimentalSDL3GameplayBindings"' in val
    assert '"ExperimentalSDL3GameplayBindings",0,ini)==1' in live
    assert '"ExperimentalSDL3NativeOwner",0,ini)==1' in live
    assert '"ExperimentalNativeJoystickMappingSwap",0,ini)==1' in live
    assert "sdl3_gameplay_live.c" in build
    assert "SpiderSDL3GameplayLive_Init(ini,logger)" in plan
    assert "SpiderSDL3GameplayLive_OnFlip(hwnd,menu2d,gameplay3d,bink,pause);" in plan
    assert main.index("SpiderSDL3GameplayLive_PreNativeOnFlip(game_hwnd);") < main.index(
        "SpiderNativeReconnect_OnFlip(count,game_hwnd);")
    assert main.index("SpiderNativeReconnect_OnFlip(count,game_hwnd);") < main.index(
        "SpiderSDL3GameplayPlan_OnFlip(game_hwnd,")
    assert 'SPIDER_GAMEPLAY_NATIVE_TABLE ((uintptr_t)0x00568408u)' in live
    assert "SPIDER_GAMEPLAY_NATIVE_DEVICE ((uintptr_t)0x006B7A2Cu)" in live
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in live
    assert "SpiderSDL3Probe_MappedCount()!=1u" in live
    assert "native_read(current) && same7(current,mapped)" in live
    assert "!same7(current,spider_ps1_buttons)" in live
    assert "spider_sdl3_native_ds4_seven(profile,desired)" in live
    assert "GetForegroundWindow()!=hwnd" in live
    assert "PAGE_READWRITE" in live
    assert "VirtualProtect(" not in live
    assert "IDirectInputDevice8_" not in live
    assert "SendInput(" not in live
    assert "WriteProcessMemory(" not in live
    assert "CreateThread(" not in live
    assert "spider_sdl3_profile_neutral(&sample)" in live
    assert "SpiderGamepadUI_SDL3GameplayPause(hwnd)" in live
    assert "SpiderSDL3GameplayLive_OwnsPause()" in ui
    assert "void SpiderGamepadUI_SDL3GameplayPause(HWND hwnd)" in ui
    assert 'begin_scancode(0x01u,"SDL3 saved Pause -> native Escape")' in ui
    assert "spider_sdl3_profile_actions(" in plan or "spider_sdl3_gameplay_plan_step(" in plan
    assert "NATIVE-SWAP: REFUSED CUSTOM joystick mapping" in native
    assert 'void SpiderSDL3GameplayLive_PreNativeOnFlip(HWND hwnd)' in live
    assert "if(applied)restore(" in live


def test_live_remains_strictly_opt_in():
    s=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    assert 'GetPrivateProfileIntA("Input",' in s
    assert 'ExperimentalSDL3GameplayBindings",0,ini)==1' in s
    assert "if(!enabled)return;" in s
    assert 'SDL3-3D-LIVE: REFUSED unsupported SDL3 binding/driver' in s
