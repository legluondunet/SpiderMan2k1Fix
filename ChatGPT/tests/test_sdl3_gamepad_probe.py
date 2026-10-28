"""SDL3 read-only multi-pad selection: compile portable policy on CI host."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not shutil.which('gcc'),reason='gcc required for C99 policy test')
def test_selection_compiles_and_handles_multi_pad_hotplug(tmp_path):
    src=tmp_path/'selection.c'
    src.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sdl3_gamepad_selection.h"
int main(void) {
    SpiderSDL3Selection s={0};
    const uint32_t one[]={11}, two[]={11,22}, three[]={11,22,33};
    const uint32_t other[]={22,33}, singleton[]={33}, dup[]={33,33};
    spider_sdl3_selection_sync(&s,NULL,0);
    assert(s.count==0 && s.active==0);
    spider_sdl3_selection_sync(&s,one,1);
    assert(s.count==1 && s.active==11);
    spider_sdl3_selection_sync(&s,two,2);
    assert(s.count==2 && s.active==11); /* don't steal focus */
    spider_sdl3_selection_start(&s,22,0);
    assert(!spider_sdl3_selection_start(&s,22,1)); /* spectator */
    spider_sdl3_selection_sync(&s,three,3);
    assert(s.active==11);
    spider_sdl3_selection_sync(&s,other,2); /* active 11 unplugged */
    assert(s.active==0);
    assert(!spider_sdl3_selection_start(&s,22,1)); /* held Start can't claim */
    assert(!spider_sdl3_selection_start(&s,22,0));
    assert(spider_sdl3_selection_start(&s,22,1));
    assert(s.active==22);
    spider_sdl3_selection_sync(&s,singleton,1);
    assert(s.count==1 && s.active==33); /* auto fallback */
    spider_sdl3_selection_sync(&s,dup,2);
    assert(s.count==1 && s.active==33); /* duplicate ignored */
    spider_sdl3_selection_sync(&s,NULL,0);
    assert(s.active==0 && s.count==0);
    /* Fresh multi-pad session must release Start before it can claim. */
    memset(&s,0,sizeof(s));
    spider_sdl3_selection_sync(&s,two,2);
    assert(!spider_sdl3_selection_start(&s,11,1));
    assert(!spider_sdl3_selection_start(&s,11,0));
    assert(spider_sdl3_selection_start(&s,11,1));
    assert(s.active==11);
    return 0;
}
''')
    exe=tmp_path/'selection'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT),str(src),'-o',str(exe)],
                   capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)

def test_sdl3_probe_is_optional_read_only_and_exe_relative():
    code=(ROOT/'sdl3_gamepad_probe.c').read_text()
    main=(ROOT/'spiderman2k1fix.c').read_text()
    shell=(ROOT/'build.sh').read_text()
    ini=(ROOT/'ini_validation.h').read_text()
    assert 'ExperimentalSDL3Probe' in ini
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in code
    assert 'SpiderSDL3Probe_OnFlip(count,game_hwnd);' in main
    assert 'SpiderSDL3Probe_Init(ini,log_line);' in main
    assert 'sdl3_gamepad_probe.c' in shell
    assert 'SDL_GetGamepads' in code and 'SDL_GetJoysticks' in code
    assert 'SDL_GetGamepadButton' in code and 'SDL_GetGamepadAxis' in code
    assert 'strcpy(slash+1,"SDL3.dll");' in code
    assert 'LoadLibraryA(path)' in code
    assert 'SpiderGamepadUI_OnFlip' in main
    for forbidden in ('SendInput(', 'WriteProcessMemory(', 'SPIDER_NATIVE_ACTION_TABLE',
                      'SetWindowLong', 'IDirectInputDevice8_'):
        assert forbidden not in code
