"""Experimental original-menu SDL3 wizard: hardware-independent safety tests.

The UI requires Windows/Wine physical verification; these host C99
capture tests and source contracts do not claim the modal popup works.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_capture_uses_fresh_neutral_and_never_assigns_ambiguous_input(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    c=tmp_path/"capture.c"
    exe=tmp_path/"capture"
    c.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_controller_capture.h"
      int main(void) {
        SpiderSDL3Capture capture={0};
        SpiderSDL3ControllerSample s={0};
        SpiderSDL3Binding got;
        /* Do not capture an inherited held face button when wizard starts. */
        spider_sdl3_capture_start(&capture);
        s.buttons=1u;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        assert(!capture.armed);
        s.buttons=0;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        assert(capture.armed);
        /* Reserved START never creates an assignment or selects a pad. */
        s.buttons=1u<<SPIDER_SDL3_BUTTON_START;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        assert(!capture.armed);
        s.buttons=0;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        assert(capture.armed);
        /* Two controls pressed simultaneously: never guess. */
        s.buttons=(1u<<1)|(1u<<2);
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        assert(!capture.armed);
        s.buttons=0;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        s.buttons=1u<<2;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==1);
        assert(got.kind==SPIDER_SDL3_BIND_BUTTON && got.index==2);
        assert(!capture.active);
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==-1);
        /* Triggers are independent and respect neutral-first protocol. */
        spider_sdl3_capture_start(&capture);
        s.buttons=0;
        s.axis[SPIDER_SDL3_AXIS_RT]=32767;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        s.axis[SPIDER_SDL3_AXIS_RT]=0;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==0);
        s.axis[SPIDER_SDL3_AXIS_LT]=32767;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==1);
        assert(got.kind==SPIDER_SDL3_BIND_TRIGGER &&
               got.index==SPIDER_SDL3_AXIS_LT);
        spider_sdl3_capture_start(&capture);
        s.axis[SPIDER_SDL3_AXIS_LT]=-1;
        assert(spider_sdl3_capture_feed(&capture,&s,&got)==-1);
        assert(!capture.armed);
        spider_sdl3_capture_reset(&capture);
        assert(!capture.active && !capture.armed);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_wizard_entry_and_native_gameplay_remain_strictly_separated():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    native=(ROOT/"ps1_joystick_readonly_menu.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    bridge=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert '"ExperimentalSDL3ControllerSetup"' in ini
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3ControllerSetup",0,ini)==1' in ui
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in ui
    assert 'SpiderSDL3ControllerSetup_Init(ini,logger);' in bridge
    assert 'sdl3_controller_setup.c' in build
    assert 'SpiderSDL3ControllerSetup_Open();' in native
    assert 'SpiderSDL3ControllerSetup_Enabled()' in native
    assert '"SDL3 TEST (PREVIEW)"' in native
    assert '0x00AC129Cu!=0u' in native
    assert '0x00AC11B4u' in native  # first column only, paired menu rows
    assert 'SpiderSDL3Probe_SetupPump()' in ui
    assert 'SpiderSDL3Probe_SetupRead(' in ui
    assert 'SpiderSDL3Probe_SetupProfile(' in ui
    assert 'spider_sdl3_capture_feed(' in ui
    assert 'spider_sdl3_profile_assign(' in ui
    assert 'spider_sdl3_profile_default(' in ui
    assert 'EnableWindow(owner,FALSE)' in ui
    assert 'EnableWindow(owner,TRUE)' in ui
    assert 'GetMessageA(&msg,NULL,0,0)' in ui
    assert 'PostQuitMessage(quit_code)' in ui
    assert 'SetForegroundWindow(owner)' in ui
    for forbidden in ("SendInput(", "IDirectInput", "VirtualProtect(",
                      "WriteFile(", "WritePrivateProfileString",
                      "CreateThread(", "PostMessageA(owner,WM_KEYDOWN"):
        assert forbidden not in ui
    setup_block=probe.split("int SpiderSDL3Probe_SetupPump(void) {",1)[1].split(
        "void SpiderSDL3Probe_OnFlip(",1)[0]
    assert 'sdl3_game_thread!=GetCurrentThreadId()' in setup_block
    assert 'sdl_pump();sdl_update();' in setup_block
    assert 'spider_sdl3_bank_get(&bindings_bank,vid,pid)' in setup_block
    for forbidden in ("SendInput(", "IDirectInput", "VirtualProtect(",
                      "WritePrivateProfileString"):
        assert forbidden not in setup_block


def test_native_joystick_capture_lock_still_on_for_gameplay():
    native=(ROOT/"ps1_joystick_readonly_menu.c").read_text(encoding="utf-8")
    assert 'memcpy(replacements[4],nop5,5u)' in native
    assert 'replacements[3][0]=0xE9u' in native
    assert 'spider_install_readonly_joystick(1u,logger)' in native
    assert 'spider_install_readonly_joystick(2u,logger)' in native
    assert 'spider_joystick_restore_denied' in native
    assert '((SpiderCMenuAddEntry)(uintptr_t)0x0043FFF0u)(native_menu,label)' in native
