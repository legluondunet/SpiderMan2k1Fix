"""Portable focus-column and Win32 SDL3 setup integration contracts.

No new game-native DirectInput mapping/joystick COM manipulation is permitted.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_column_navigation_and_three_right_buttons_are_pure_c99(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    code=tmp_path/"focus.c"
    exe=tmp_path/"focus"
    code.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_controller_setup_focus.h"
        #define STEP(b) spider_sdl3_setup_focus_step(&f,1u<<(b))
        int main(void) {
          SpiderSDL3SetupFocus f={0};
          spider_sdl3_setup_focus_reset(&f);
          assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
          assert(!STEP(SPIDER_SETUP_NAV_LEFT));
          assert(!STEP(SPIDER_SETUP_NAV_DOWN)); /* action rows external */
          assert(STEP(SPIDER_SETUP_NAV_RIGHT));
          assert(f.column==SPIDER_SETUP_FOCUS_BUTTONS && f.button==0);
          assert(!STEP(SPIDER_SETUP_NAV_RIGHT));
          assert(STEP(SPIDER_SETUP_NAV_DOWN));
          assert(f.button==SPIDER_SETUP_BUTTON_RESET);
          assert(STEP(SPIDER_SETUP_NAV_DOWN));
          assert(f.button==SPIDER_SETUP_BUTTON_CLOSE);
          assert(STEP(SPIDER_SETUP_NAV_DOWN));
          assert(f.button==SPIDER_SETUP_BUTTON_CAPTURE);
          assert(STEP(SPIDER_SETUP_NAV_UP));
          assert(f.button==SPIDER_SETUP_BUTTON_CLOSE);
          assert(STEP(SPIDER_SETUP_NAV_LEFT));
          assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
          assert(f.button==SPIDER_SETUP_BUTTON_CLOSE);
          assert(!STEP(SPIDER_SETUP_NAV_UP));
          assert(STEP(SPIDER_SETUP_NAV_RIGHT));
          assert(f.column==SPIDER_SETUP_FOCUS_BUTTONS);
          assert(f.button==SPIDER_SETUP_BUTTON_CLOSE);
          assert(!STEP(SPIDER_SETUP_NAV_SOUTH));
          assert(!STEP(SPIDER_SETUP_NAV_EAST));
          assert(!STEP(SPIDER_SETUP_NAV_NORTH));
          assert(!STEP(SPIDER_SETUP_NAV_LB));
          assert(!STEP(SPIDER_SETUP_NAV_RB));
          spider_sdl3_setup_focus_reset(&f);
          assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS && f.button==0);
          assert(!spider_sdl3_setup_focus_step((SpiderSDL3SetupFocus*)0,
                     1u<<SPIDER_SETUP_NAV_RIGHT));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(code),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_win32_focus_and_deferred_activation_guards():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    nav=(ROOT/"sdl3_controller_setup_nav.h").read_text(encoding="utf-8")
    focus=(ROOT/"sdl3_controller_setup_focus.h").read_text(encoding="utf-8")
    assert '#include "sdl3_controller_setup_focus.h"' in ui
    assert 'SpiderSDL3SetupFocus focus;' in ui
    assert 'SPIDER_SETUP_NAV_LEFT=21, SPIDER_SETUP_NAV_RIGHT=22' in nav
    assert 'analog|=1u<<SPIDER_SETUP_NAV_LEFT' in nav
    assert 'analog|=1u<<SPIDER_SETUP_NAV_RIGHT' in nav
    assert 'analog|=1u<<SPIDER_SETUP_NAV_LB' not in nav
    assert 'analog|=1u<<SPIDER_SETUP_NAV_RB' not in nav
    assert 'SPIDER_SETUP_BUTTON_COUNT=3' in focus
    assert 'static void setup_apply_focus(SpiderSDL3SetupUI *ui)' in ui
    assert 'WM_CHANGEUISTATE' in ui
    assert 'UISF_HIDEFOCUS' in ui
    assert 'SetFocus(target);' in ui
    assert 'GetDlgItem(ui->hwnd,setup_focus_button_id(ui->focus.button))' in ui
    assert 'FOCUS: %s. Cross/A activates;' in ui
    assert 'if(spider_sdl3_setup_focus_step(&ui->focus,edge))' in ui
    assert 'if(ui->focus.column==SPIDER_SETUP_FOCUS_BUTTONS)' in ui
    assert 'SPIDER_SETUP_NAV_LB' in ui and 'SPIDER_SETUP_NAV_RB' in ui
    assert 'setup_close_from_pad(ui,' in ui
    assert 'Cross/A activated focused CLOSE; waiting for release' in ui
    assert 'PostMessageA(ui->hwnd,WM_COMMAND,' in ui
    assert 'MAKEWPARAM(id,BN_CLICKED)' in ui
    assert 'if(ui->close_pending) {' in ui
    assert 'spider_sdl3_profile_neutral(&s)' in ui
    assert 'PostMessageA(ui->hwnd,WM_CLOSE,0,0);' in ui
    assert "if(!ui->capture.active) {" in ui
    assert 'rc=spider_sdl3_capture_feed(&ui->capture,&s,&candidate)' in ui
    assert "case WM_TIMER:" in ui
    assert 'HIWORD(wp)==BN_SETFOCUS' in ui
    assert 'HIWORD(wp)==LBN_SETFOCUS' in ui
    assert ui.count("WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_NOTIFY") == 3
    # A physical East/Circle is still assignable to a 3D action during
    # explicit capture; ESC remains the non-ambiguous cancel operation.
    capture=(ROOT/"sdl3_controller_capture.h").read_text(encoding="utf-8")
    assert 'spider_sdl3_capture_feed(' in capture
    assert 'b=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,i);n++;' in capture
    assert 'vk==VK_ESCAPE' in ui
    assert 'if(ui->capture.active)' in ui
    for forbidden in ("SendInput(", "AttachThreadInput(",
                      "IDirectInputDevice8_Acquire(", "WriteProcessMemory("):
        assert forbidden not in ui
