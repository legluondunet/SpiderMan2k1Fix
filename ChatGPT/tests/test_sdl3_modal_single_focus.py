"""SDL3 setup Win32 single-cursor and explicit remap action invariants."""
from pathlib import Path
import shutil
import subprocess
import tempfile

import pytest

ROOT=Path(__file__).resolve().parents[2]
UI=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
FOCUS=(ROOT/"sdl3_controller_setup_focus.h").read_text(encoding="utf-8")


def test_only_one_selection_cursor_and_use_is_not_action_capture():
    assert "static void setup_sync_visual_selection(" in UI
    assert "LB_SETCURSEL," in UI
    assert "(WPARAM)ui->action:(WPARAM)-1" in UI
    assert "SPIDER_SETUP_BUTTON_USE" in FOCUS
    assert "SPIDER_SETUP_BUTTON_COUNT=6" in FOCUS
    assert "SPIDER_UI_USE;" in UI
    assert "SPIDER_SETUP_BUTTON_RAW+LOWORD(wp)-SPIDER_UI_RAW" in UI
    assert "HIWORD(wp)==BN_SETFOCUS" in UI
    assert UI.count("WS_TABSTOP|BS_PUSHBUTTON|BS_NOTIFY")>=3
    selector=UI.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;" in selector
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in selector
    assert "SetFocus(ui->actions)" in selector
    assert "EXPLICIT-SELECT already queued; duplicate activation ignored" in selector
    assert "ui->start_owner_requested=1u;" in selector
    assert "spider_sdl3_capture_start(" not in selector
    nav=UI.split("static void sample_device(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    assert "South on actions list ignored (explicit Capture button required)" in nav
    assert "spider_sdl3_capture_start(&ui->capture);" not in nav
    keyboard=UI.split("static int handle_setup_keyboard(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "Ctrl+Shift+C" in UI
    assert "if(vk=='C'" in keyboard
    assert "if(focused==ui->actions)" in keyboard
    assert "if(focused==ui->actions && current_profile(ui))" not in keyboard
    # No direct remapping by Enter/Space unless an explicit Capture button
    # (or Ctrl+Shift+C) was invoked.
    assert "if(ui->focus.column!=SPIDER_SETUP_FOCUS_ACTIONS)" in keyboard
    assert "Action selected. Press Enter to capture" not in UI


@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="No C compiler")
def test_six_button_focus_navigation_in_real_portable_policy():
    source=r"""
#include <assert.h>
#include "sdl3_controller_setup_focus.h"
int main(void) {
  SpiderSDL3SetupFocus f={0};
  unsigned i;
  assert(SPIDER_SETUP_BUTTON_COUNT==6);
  spider_sdl3_setup_focus_reset(&f);
  assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_RIGHT));
  assert(f.column==SPIDER_SETUP_FOCUS_BUTTONS);
  for(i=0;i<5;i++)
    assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_DOWN));
  assert(f.button==SPIDER_SETUP_BUTTON_USE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_DOWN));
  assert(f.button==SPIDER_SETUP_BUTTON_CAPTURE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_UP));
  assert(f.button==SPIDER_SETUP_BUTTON_USE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_LEFT));
  assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
  assert(!spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_LEFT));
  return 0;
}
"""
    with tempfile.TemporaryDirectory() as tmp:
        src=Path(tmp)/"focus.c"
        exe=Path(tmp)/"focus"
        src.write_text(source,encoding="utf-8")
        subprocess.run([shutil.which("cc") or shutil.which("gcc"),
            "-std=c11","-Wall","-Wextra","-Werror",
            "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
