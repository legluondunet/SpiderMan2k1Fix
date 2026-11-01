"""Optional universal SDL3 gameplay bridge: C99 semantics and fail-closed ownership.

These are portable policy/source tests; they DO NOT replace hardware
validation of original game-owned DirectInput pointer suspension or of
SendInput in retail 3D on Windows and Wine.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_universal_profile_actions_axes_dpad_and_pause_edges(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"universal.c"
    exe=tmp_path/"universal"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        SpiderSDL3UniversalState state={0};
        SpiderSDL3ControllerProfile profile;
        SpiderSDL3ControllerSample sample={0};
        uint32_t desired=999u;
        unsigned pause=999u;
        assert(spider_sdl3_profile_default(&profile,0xfffeu,0x1234u));
        /* Physical SDL3 south is Jump regardless of manufacturer. */
        sample.buttons=1u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==0);
        assert(desired==0u && pause==0u);
        sample.buttons=0;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==0);
        assert(state.armed && !desired);
        sample.buttons=1u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_JUMP) && pause==0);
        sample.buttons=0;
        sample.axis[SPIDER_SDL3_AXIS_LEFTX]=22000;
        sample.axis[SPIDER_SDL3_AXIS_LEFTY]=-21000;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_RIGHT |
                         1u<<SPIDER_UNIVERSAL_UP));
        /* D-pad must be generic SDL button 11..14, not physical DI index. */
        sample.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
        sample.axis[SPIDER_SDL3_AXIS_LEFTY]=0;
        sample.buttons=(1u<<SPIDER_UNIVERSAL_DPAD_DOWN) |
                       (1u<<SPIDER_UNIVERSAL_DPAD_LEFT);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_DOWN |
                         1u<<SPIDER_UNIVERSAL_LEFT));
        /* Pause is a separate rising edge and never a held gameplay key. */
        sample.buttons=1u<<4;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==0u && pause==1u);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,1,
                                         &desired,&pause)==1);
        assert(!desired && !pause);
        sample.buttons=0u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        sample.buttons=1u<<4;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(pause==1u);
        /* A different SDL3 instance must never inherit held buttons. */
        assert(spider_sdl3_universal_step(&state,&profile,&sample,18,0,
                                         &desired,&pause)==0);
        assert(!desired && !pause && !state.armed);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_universal_runtime_is_opt_in_and_mutually_exclusive():
    game=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    cfg=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert 'ExperimentalSDL3UniversalGameplay=0' in cfg
    assert '"ExperimentalSDL3UniversalGameplay"' in ini
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)==1' in backend
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)!=1' in live
    assert 'sdl3_universal_gameplay.c' in build
    assert '#include "sdl3_universal_gameplay.h"' in game
    assert 'SpiderSDL3Universal_BeforeUI(game_hwnd);' in game
    assert game.index("SpiderSDL3Probe_OnFlip(count,game_hwnd);") < game.index(
        "SpiderSDL3Universal_BeforeUI(game_hwnd);") < game.index(
        "SpiderGamepadUI_OnFlip(game_hwnd")
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in game
    assert "SpiderSDL3Universal_SuppressesNative()" in native
    assert "SpiderSDL3Universal_Enabled()" in ui
    assert "SpiderSDL3Raw_ProfileKey" in probe
    assert "SpiderSDL3Probe_SelectedMappedSample" in probe
    assert "InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT" in backend
    assert "restored" in backend.lower()
    assert "release_all()" in backend
    assert "SpiderCfg_NativeConfigLoaded()" in backend
    assert "spider_sdl3_universal_step" in backend
    assert "SPIDER_UNIVERSAL_DI_SLOT" in backend
    assert "KEYEVENTF_SCANCODE" in backend
    assert "KEYEVENTF_EXTENDEDKEY" in backend
    for forbidden in ("VirtualProtect(", "WriteProcessMemory(",
                      "IDirectInputDevice8_Release(", "IDirectInputDevice8_Acquire(",
                      "SetWindowsHookEx(", "CreateThread("):
        assert forbidden not in backend
