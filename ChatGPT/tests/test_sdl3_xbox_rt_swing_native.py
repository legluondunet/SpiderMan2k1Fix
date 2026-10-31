"""Xbox RT Swing: preserve legacy mode; SDL3 can unbind BUTTON9 safely.

Only Windows hardware can confirm that injected native keyboard Swing
produces the actual web-swing action.  These tests prevent a dangerous
unverified analog-to-native button-index guess and stuck states.
"""
from pathlib import Path
import shutil,subprocess,textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="host C99 compiler needed")
def test_rt_only_native_xbox_action_table_preserves_sentinel_and_old_preset(tmp_path):
    p=tmp_path/"rt_map.c"
    p.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "native_joystick_swap_probe.h"
      int main(void) {
        unsigned i;
        uint8_t raw[SPIDER_NATIVE_SWAP_SLOTS*8]={0};
        uint32_t map[SPIDER_PS1_BUTTON_COUNT]={0};
        for(i=0;i<SPIDER_NATIVE_SWAP_SLOTS;i++) {
          spider_ps1_write_word(raw,8*i,spider_native_swap_masks[i]);
          spider_ps1_write_word(raw,8*i+4,0x4000u);
        }
        spider_ps1_write_word(raw,13*8+4,0u);
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          spider_ps1_write_word(raw,(4+i)*8+4,spider_xbox360_buttons_sdl3_rt[i]);
        assert(spider_native_swap_read(raw,sizeof(raw),map));
        assert(spider_native_swap_preset(map)==SPIDER_RECONNECT_XBOX360);
        assert(map[6]==SPIDER_XBOX360_NATIVE_UNBOUND);
        assert(map[6]==0x4000u); /* original engine's native unused marker */
        assert(map[5]==5u && map[0]==0u);
        for(i=0;i<SPIDER_PS1_BUTTON_COUNT;i++)
          spider_ps1_write_word(raw,(4+i)*8+4,spider_xbox360_buttons[i]);
        assert(spider_native_swap_read(raw,sizeof(raw),map));
        assert(spider_native_swap_preset(map)==SPIDER_RECONNECT_XBOX360);
        assert(map[6]==9u); /* legacy fallback not changed */
        return 0;
      }
    """).replace("SPIDER_RECONNECT_XBOX360","2u"),encoding="utf-8")
    binary=tmp_path/"rt_map"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
        "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
        str(p),"-o",str(binary)],check=True,capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)

def test_rt_only_output_requires_verified_holtek_caps_and_sdl3_owner():
    native=(ROOT/"native_joystick_reconnect.c").read_text()
    ui=(ROOT/"gamepad_ui_profile.c").read_text()
    val=(ROOT/"ini_validation.h").read_text()
    mapping=(ROOT/"xbox360_gameplay_mapping.h").read_text()
    assert "spider_xbox360_buttons_sdl3_rt" in mapping
    assert "SPIDER_XBOX360_NATIVE_UNBOUND" in mapping
    assert "xbox_sdl3_rt=to_kind==SPIDER_RECONNECT_XBOX360" in native
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in native
    assert "selected_vid==0x045eu && selected_pid==0x028eu" in native
    assert "SPIDER_NATIVE_BUTTON_COUNT==10u" in native
    assert "xbox_sdl3_rt?spider_xbox360_buttons_sdl3_rt:" in native
    assert "memcmp(current,target,sizeof(current))==0" in native
    assert '"ExperimentalSDL3XboxRTSwing",1,ini)==1' in ui
    assert "xbox_rt_swing_requested=xbox_rt_swing_sdl3_mode ||" in ui
    assert "spider_xbox360_rt_swing_held_any(" in ui
    assert "SpiderSDL3Probe_SelectedRT(&sdl3_rt)" in ui
    assert "xbox_rt_swing_release(\"focus lost\");" in ui
    assert "xbox_rt_swing_release(\"secondary controller disabled\");" in ui
    assert '"ExperimentalSDL3XboxRTSwing"' in val

def test_old_audio_option_is_accepted_without_reenabling_audio_router():
    val=(ROOT/"ini_validation.h").read_text()
    assert '"DualShock4AudioOutput"' in val
    assert "audio_ds4_route.c" not in (ROOT/"build.sh").read_text()
