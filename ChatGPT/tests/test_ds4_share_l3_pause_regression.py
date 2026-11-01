"""DS4 SHARE/L3 native pause regression: portable table + source safety gates.

The physical Wine/Windows controller test is still required. No test here
claims to validate runtime DirectInput device access or synthetic Escape.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[2]


def test_verified_ds4_l3_is_not_the_sdl3_pause_owner():
    live = (ROOT / "sdl3_gameplay_live.c").read_text(encoding="utf-8")
    native = (ROOT / "native_joystick_reconnect.c").read_text(encoding="utf-8")
    ui = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
    assert "PAGE_WRITECOPY" in live
    assert "(applied || paused_edge_armed)" in live
    assert "native action table unreadable or unsupported protection/layout" in live
    assert "spider_sdl3_disable_ds4_l3_native_pause();" in native
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in native
    assert "SPIDER_NATIVE_BUTTON_COUNT!=14u" in native
    assert "spider_native_swap_preset(buttons)!=SPIDER_RECONNECT_DS4" in native
    assert "InterlockedCompareExchange((volatile LONG *)slot,0x4000L,11L)" in native
    assert "spider_native_swap_read(" in native
    assert "vid!=0x054cu" in native
    assert "PAGE_WRITECOPY" in native
    assert "spider_sdl3_ds4_native_button" not in native
    assert "begin_scancode(0x01u" not in native
    assert "!SpiderSDL3GameplayLive_OwnsPause()" in ui
    assert "SDL3-DS4: native Start BUTTON11" in native


def test_native_l3_start_slot_unbind_preserves_seven_actions(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    code = tmp_path / "ds4_l3.c"
    exe = tmp_path / "ds4_l3"
    code.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdint.h>
      #include <string.h>
      #include "native_joystick_swap_probe.h"
      int main(void) {
        uint8_t raw[SPIDER_NATIVE_SWAP_SLOTS*8u]={0};
        uint32_t read_buttons[SPIDER_PS1_BUTTON_COUNT]={0};
        unsigned i;
        for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
          spider_ps1_write_word(raw,i*8u,spider_native_swap_masks[i]);
          spider_ps1_write_word(raw,i*8u+4u,i==13u?0u:0x4000u);
        }
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          spider_ps1_write_word(raw,(i+SPIDER_NATIVE_SWAP_FIRST_ACTION)*8u+4u,
                                spider_ps1_buttons[i]);
        /* Exactly the observed original native Start BUTTON11. */
        spider_ps1_write_word(raw,11u*8u+4u,11u);
        assert(spider_native_swap_read(raw,sizeof(raw),read_buttons));
        assert(spider_native_swap_preset(read_buttons)==1u);
        assert(spider_ps1_read_word(raw,11u*8u+4u)==11u);
        /* This single RAM change must not affect the seven actions. */
        spider_ps1_write_word(raw,11u*8u+4u,0x4000u);
        memset(read_buttons,0,sizeof(read_buttons));
        assert(spider_native_swap_read(raw,sizeof(raw),read_buttons));
        assert(spider_native_swap_preset(read_buttons)==1u);
        assert(!memcmp(read_buttons,spider_ps1_buttons,sizeof(read_buttons)));
        return 0;
      }
    """), encoding="utf-8")
    subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(code), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
