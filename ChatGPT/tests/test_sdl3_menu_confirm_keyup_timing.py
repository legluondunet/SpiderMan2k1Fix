"""Menu South/A/Cross must reach retail DirectInput Save Game confirmation.

Portable timing policy and source-isolation checks only. Exact Wine/Windows
game reception needs a hardware run with the Save Game screen.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_menu_confirm_deferred_release_is_strictly_one_extra_flip(tmp_path):
    source=tmp_path/"menu_confirm.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        unsigned frames=1u;
        assert(spider_sdl3_menu_confirm_defer_keyup(&frames));
        assert(frames==0u);
        assert(!spider_sdl3_menu_confirm_defer_keyup(&frames));
        assert(!spider_sdl3_menu_confirm_defer_keyup(0));
        frames=0u;
        assert(!spider_sdl3_menu_confirm_defer_keyup(&frames));
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"menu_confirm"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe)],check=True)


def test_save_game_confirm_holds_input_without_changing_video_or_pause():
    source=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    before=source.split("void SpiderSDL3Universal_BeforeUI(",1)[1].split(
        "int SpiderSDL3Universal_ReleaseSyntheticReturnBefore3D(",1)[0]
    menu=source.split("if(context!=SPIDER_UNIVERSAL_CTX_3D) {",1)[1].split(
        "  menu_arm=0;menu_previous=0;",1)[0]
    assert 'if(pulse(0x1cu,"menu Confirm"))' in menu
    assert 'if(context==SPIDER_UNIVERSAL_CTX_MENU)' in menu
    assert "menu_confirm_hold_flips=1u;" in menu
    assert "menu_confirm_pending=1;" in menu
    assert "menu_arm=1;" in menu
    assert "menu_confirm_unarmed_reported=1;" in menu
    assert 'pulse(0x01u,"menu Back/Pause resume")' in menu
    assert 'pulse(0x1cu,"menu Confirm")' in menu
    assert "spider_sdl3_menu_confirm_defer_keyup(" in before
    assert before.index("if(pulse_scan && menu_confirm_pending &&") < (
        before.index("else if(pulse_scan && !send_scan(pulse_scan,1))"))
    assert "GetForegroundWindow()!=hwnd" in before
    assert "restore_original(\"focus or window lost\")" in before
    assert "menu_confirm_hold_flips=menu_confirm_pending=0;" in before
    assert "menu_confirm_hold_flips=0;" in source.split(
        "static int release_all(void)",1)[1].split(
        "static int update_keys(",1)[0]
    assert "restore_original(\"menu Confirm KEYUP failed on context change\")" in source
    assert "menu_confirm_hold_flips=menu_confirm_pending=0;" in source.split(
        "int SpiderSDL3Universal_ReleaseSyntheticReturnBefore3D(",1)[1].split(
        "static int binding_held(",1)[0]
    assert 'pulse(0x1cu,"native GAME OVER Return confirmation")' in source
    assert 'pulse(0x01u,"3D Pause")' in source
    assert "SDL3-MENU-CONFIRM: South/Confirm injected Enter KEYDOWN" in source
    assert "SDL3-MENU-CONFIRM: injected Enter KEYUP accepted by OS" in source
    assert "SDL3-MENU-CONFIRM: South/Confirm ignored" in source
