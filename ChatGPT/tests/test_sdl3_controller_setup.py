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
    assert 'spider_sdl3_profile_assign_visible(' in ui
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


def test_setup_navigation_requires_neutral_press_edges_and_selection_reset(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    c=tmp_path/"nav.c"
    exe=tmp_path/"nav"
    c.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_controller_setup_nav.h"
      int main(void) {
        SpiderSDL3SetupNav nav={0};
        SpiderSDL3ControllerSample pad={0};
        uint32_t edge;
        /* Physical A already held at dialog open cannot capture instantly. */
        pad.buttons=1u<<SPIDER_SETUP_NAV_SOUTH;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(!nav.armed);
        pad.buttons=0;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(nav.armed);
        pad.buttons=1u<<SPIDER_SETUP_NAV_SOUTH;
        edge=spider_sdl3_setup_nav_step(&nav,&pad);
        assert(edge==(1u<<SPIDER_SETUP_NAV_SOUTH));
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        pad.buttons=0;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        pad.buttons=1u<<SPIDER_SETUP_NAV_DOWN;
        assert(spider_sdl3_setup_nav_step(&nav,&pad)==
               (1u<<SPIDER_SETUP_NAV_DOWN));
        pad.buttons=0;
        spider_sdl3_setup_nav_step(&nav,&pad);
        pad.buttons=(1u<<SPIDER_SETUP_NAV_SOUTH)|
                    (1u<<SPIDER_SETUP_NAV_EAST);
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(!nav.armed);
        pad.buttons=0;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        /* Gamepad switch Start stays reserved and forces neutral. */
        pad.buttons=1u<<SPIDER_SDL3_BUTTON_START;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(!nav.armed);
        pad.buttons=0;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        pad.buttons=1u<<SPIDER_SETUP_NAV_RB;
        assert(spider_sdl3_setup_nav_step(&nav,&pad)==
               (1u<<SPIDER_SETUP_NAV_RB));
        pad.buttons=0;
        spider_sdl3_setup_nav_reset(&nav);
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(nav.armed);
        /* Invalid trigger data fails closed and resets input state. */
        pad.axis[SPIDER_SDL3_AXIS_RT]=-1;
        pad.buttons=1u<<SPIDER_SETUP_NAV_NORTH;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        assert(!nav.armed);
        pad.axis[SPIDER_SDL3_AXIS_RT]=0;
        pad.buttons=0;
        assert(!spider_sdl3_setup_nav_step(&nav,&pad));
        pad.buttons=1u<<SPIDER_SETUP_NAV_EAST;
        assert(spider_sdl3_setup_nav_step(&nav,&pad)==
               (1u<<SPIDER_SETUP_NAV_EAST));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_mouse_free_keyboard_and_selected_gamepad_setup_navigation():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    nav=(ROOT/"sdl3_controller_setup_nav.h").read_text(encoding="utf-8")
    assert 'static int handle_setup_keyboard(' in ui
    assert 'handle_setup_keyboard(&ui,(UINT)msg.wParam)' in ui
    assert 'SetFocus(ui.actions); /* Keyboard-first' in ui
    for key in ("VK_UP","VK_DOWN","VK_RETURN","VK_PRIOR","VK_NEXT",
                "VK_F5","VK_ESCAPE","VK_F1"):
        assert key in ui
    for action in ("SPIDER_SETUP_NAV_UP","SPIDER_SETUP_NAV_DOWN",
                   "SPIDER_SETUP_NAV_LB","SPIDER_SETUP_NAV_RB",
                   "SPIDER_SETUP_NAV_SOUTH","SPIDER_SETUP_NAV_EAST",
                   "SPIDER_SETUP_NAV_NORTH"):
        assert action in ui
    assert 'spider_sdl3_setup_nav_step(&ui->nav,&s)' in ui
    assert 'spider_sdl3_setup_nav_reset(&ui->nav)' in ui
    assert 'if(!ui->capture.active) {' in ui
    assert 'if(rc!=1)return;' in ui
    assert 'GetForegroundWindow()!=ui->hwnd' in ui
    assert 'if(focused==ui->pads)' in ui
    assert 'if(focused==GetDlgItem(ui->hwnd,SPIDER_UI_RESET)' in ui
    assert 'SPIDER_SETUP_NAV_SOUTH=0' in nav
    assert 'SPIDER_SETUP_NAV_EAST=1' in nav
    assert 'SPIDER_SETUP_NAV_UP=11' in nav
    assert 'SPIDER_SETUP_NAV_DOWN=12' in nav
    assert 'spider_sdl3_profile_neutral(sample)' in nav
    assert 'spider_sdl3_setup_nav_reset(n);' in nav
    assert not any(ord(ch)>127 for ch in ui), "ANSI game dialog must use ASCII-only labels"
    assert 'Keyboard: Up/Down action' in ui
    assert 'Gamepad: D-pad action' in ui
    for forbidden in ("SendInput(", "WritePrivateProfileString",
                      "VirtualProtect(", "CreateThread("):
        assert forbidden not in ui


def test_modal_close_cannot_destroy_dialog_inside_sdl3_timer_callback():
    """Guard candidate fix after Wine crash immediately following reset."""
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    timer=ui.split("static void sample_device(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    assert "ui->close_pending=1;" in timer
    assert "ui->close_requested_ms=GetTickCount();" in timer
    assert "spider_sdl3_profile_neutral(&s)" in timer
    assert "GetTickCount()-ui->close_requested_ms" in timer
    assert "PostMessageA(ui->hwnd,WM_CLOSE,0,0);" in timer
    assert "SendMessageA(ui->hwnd,WM_CLOSE,0,0)" not in timer
    assert "DestroyWindow(" not in timer
    assert "selected pad disconnected while CLOSE pending" in timer
    assert "SDL3-SETUP: Reset BEGIN" in ui
    assert "SDL3-SETUP: WM_CLOSE entered" in ui
    assert "SDL3-SETUP: WM_DESTROY done" in ui
    assert "SDL3-SETUP: modal message loop EXIT" in ui
    assert "SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);" in ui
    assert "KillTimer(hwnd,SPIDER_UI_TIMER);" in ui
    keyboard=ui.split("static int handle_setup_keyboard(",1)[1].split(
        "static LRESULT CALLBACK setup_wndproc",1)[0]
    assert "PostMessageA(ui->hwnd,WM_CLOSE,0,0);" in keyboard
    assert "SendMessageA(ui->hwnd,WM_CLOSE,0,0)" not in keyboard
    handler=ui.split("static LRESULT CALLBACK setup_wndproc",1)[1].split(
        "void SpiderSDL3ControllerSetup_Open",1)[0]
    assert "PostMessageA(hwnd,WM_CLOSE,0,0);" in handler
    assert "DestroyWindow(hwnd);" in handler.split("case WM_CLOSE:",1)[1]
    assert "DestroyWindow(hwnd);" not in handler.split("case WM_COMMAND:",1)[1].split("case WM_TIMER:",1)[0]
